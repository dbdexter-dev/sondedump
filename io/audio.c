#include <portaudio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <windows.h>
#else
#include <semaphore.h>
#endif
#include "audio.h"
#include "log/log.h"
#include "utils.h"

static PaError print_error(PaError err);
static int audio_stop_stream(void);
static int pa_callback(const void *in, void *out, unsigned long frame_count, const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags status, void *user_data);

static const double _samplerates[] = {48000, 44100};
static struct {
	PaStream *stream;
	float buffer[BUFFER_SIZE];
	unsigned long head, tail;

#ifdef _MSC_VER
	HANDLE sem_rdy;
#else
	sem_t sem_rdy;
#endif

	const char **device_names;
	int *device_idxs;
	int device_count;
} _state;

int
audio_init()
{
	const PaDeviceInfo *dev_info;
	PaError err;
	int i;
	int dev_count;

	_state.head = 0;
	_state.tail = 0;

	/* Initialize portaudio */
	if ((err = Pa_Initialize()) != paNoError) {
		return print_error(err);
	}

	/* Query devices */
	if ((dev_count = Pa_GetDeviceCount()) < 0) {
		log_error("Pa_GetDeviceCount returned %d", dev_count);
		return dev_count;
	}

	_state.device_count = 0;
	_state.device_names = malloc(sizeof(*_state.device_names) * dev_count);
	_state.device_idxs = malloc(sizeof(*_state.device_idxs) * dev_count);

	/* Enumerate devices and collect their names */
	for (i=0; i<dev_count; i++) {
		dev_info = Pa_GetDeviceInfo(i);

		if (dev_info->maxInputChannels > 0) {
			_state.device_names[_state.device_count] = dev_info->name;
			_state.device_idxs[_state.device_count] = i;
			_state.device_count++;
		}
	}

	_state.stream = NULL;

	/* Initialize callback-to-main signalling */
#ifdef _MSC_VER
	_state.sem_rdy = CreateSemaphore(NULL, 0, 0xFFFF, NULL);
#else
	sem_init(&_state.sem_rdy, 0, 0);
#endif

	return 0;
}

int
audio_open_device(int device_idx)
{
	PaStream *new_stream;

	const PaDeviceInfo *dev_info;
	PaStreamParameters input_params = {
		.device = 0,
		.channelCount = 1,
		.sampleFormat = paFloat32,
		.suggestedLatency = 0,
		.hostApiSpecificStreamInfo = NULL
	};
	int i, err;
	double samplerate;

	/* Make sure that any previous stream is stopped and deinitialized */
	audio_stop_stream();

	if (device_idx >= _state.device_count) {
		return -2;
	}

	/* Translate device index to portaudio index */
	device_idx = _state.device_idxs[device_idx];

	/* Attempt to open the device as a mono input, zero channel output. Try a
	 * bunch of different, known working sample rates */
	dev_info = Pa_GetDeviceInfo(device_idx);
	input_params.device = device_idx;
	input_params.suggestedLatency = dev_info->defaultHighInputLatency;

	samplerate = -1;
	for (i=0; i<(int)LEN(_samplerates); i++) {
		if (Pa_IsFormatSupported(&input_params, NULL, _samplerates[i]) == paFormatIsSupported) {
			samplerate = _samplerates[i];
			break;
		}
	}
	if (samplerate < 0) {
		log_error("Could not initialize specified device with sane parameters");
		_state.stream = NULL;
		return -1;
	}

	/* Open stream */
	err = Pa_OpenStream(&new_stream,
	                    &input_params,
	                    NULL,                                   /* Input-only stream */
	                    samplerate,                             /* Agreed upon samplerate */
	                    BUFFER_SIZE/4,                          /* Very high frame size reduces CPU usage */
	                    paNoFlag | paDitherOff | paClipOff,     /* Disable extra audio processing */
	                    pa_callback,                            /* Callback function */
	                    NULL
	);
	if (err != paNoError) {
		return print_error(err);
	}

	/* Start streaming audio from the device */
	if ((err = Pa_StartStream(new_stream)) != paNoError) {
		return print_error(err);
	}

	/* Install new stream ptr atomically */
	_state.stream = new_stream;

	return (int)samplerate;
}

int
audio_deinit(void)
{
	audio_stop_stream();

	Pa_Terminate();

#ifdef _MSC_VER
	CloseHandle(_state.sem_rdy);
#else
	sem_destroy(&_state.sem_rdy);
#endif
	_state.device_count = 0;
	free(_state.device_names);
	free(_state.device_idxs);

	return 0;
}

int
audio_read(float *ptr, size_t count)
{
	size_t valid_count, first_read, second_read;

	/* If stream is not yet open, exit */
	if (!_state.stream || Pa_IsStreamStopped(_state.stream)) {
		return 1;
	}

	/* While there's data left to read */
	while (count) {
		/* Get number of valid samples in the ring buffer */
		valid_count = (_state.head - _state.tail + LEN(_state.buffer)) % LEN(_state.buffer);
		first_read = MIN(MIN(valid_count, count), LEN(_state.buffer) - _state.tail);

		/* First chunk, up to end of buffer */
		memcpy(ptr, _state.buffer + _state.tail, sizeof(*_state.buffer) * first_read);
		_state.tail = (_state.tail + first_read) % LEN(_state.buffer);

		ptr += first_read;
		second_read = MIN(count, valid_count) - first_read;

		/* Second chunk, from start to whatever is left (optional) */
		memcpy(ptr, _state.buffer, sizeof(*_state.buffer) * second_read);
		_state.tail = (_state.tail + second_read) % LEN(_state.buffer);

		count -= MIN(count, valid_count);
		ptr += second_read;

		/* Wait for more data to be available */
		if (count) {
#ifdef _MSC_VER
			WaitForSingleObject(_state.sem_rdy, INFINITE);
#else
			sem_wait(&_state.sem_rdy);
#endif
		}
	}

	return 1;
}

int
audio_get_num_devices(void)
{
	return _state.device_count;
}

const char**
audio_get_device_names(void)
{
	return _state.device_names;
}

/* Static functions {{{ */
static int
pa_callback(const void *in, void *out, unsigned long frame_count, const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags status, void *user_data)
{
	unsigned int first_write;
	const float *in_float = (const float*)in;

	(void)out;
	(void)user_data;
	(void)time_info;
	(void)status;

	/* First chunk, up to end of ring buffer */
	first_write = MIN(frame_count, LEN(_state.buffer) - _state.head);

	memcpy(_state.buffer + _state.head, in_float, sizeof(*_state.buffer) * first_write);
	_state.head = (_state.head + first_write) % LEN(_state.buffer);

	frame_count -= first_write;

	/* Second chunk, up to end of input */
	memcpy(_state.buffer, in_float + first_write, sizeof(*_state.buffer) * frame_count);
	_state.head = (_state.head + frame_count) % LEN(_state.buffer);

#ifdef _MSC_VER
	ReleaseSemaphore(_state.sem_rdy, 1, NULL);
#else
	sem_post(&_state.sem_rdy);
#endif
	return paContinue;
}

static PaError
print_error(PaError err)
{
	log_error("Portaudio error: %s", Pa_GetErrorText(err));
	return err;
}

static int
audio_stop_stream(void)
{
	int err;
	if (!_state.stream) return 0;

	/* Stop streaming audio and deinitialize stream */
	if ((err = Pa_AbortStream(_state.stream))) {
		print_error(err);
	}
	_state.stream = NULL;

	return 0;
}

/* }}} */
