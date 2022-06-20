#include <portaudio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "audio.h"
#include "utils.h"

static PaError print_error(PaError err);
static int audio_stop_stream(void);

static const double _samplerates[] = {48000, 44100};
static struct {
	PaStream *stream;
	float buffer[BUFFER_SIZE];
	unsigned long idx;

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

	_state.idx = 0;

	/* Initialize portaudio */
	if ((err = Pa_Initialize()) != paNoError) {
		return print_error(err);
	}

	/* Query devices */
	if ((dev_count = Pa_GetDeviceCount()) < 0) {
		fprintf(stderr, "[ERROR] Pa_GetDeviceCount returned %d\n", dev_count);
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
		fprintf(stderr, "[ERROR] Could not initialize specified device with sane parameters\n");
		_state.stream = NULL;
		return -1;
	}

	/* Open stream */
	err = Pa_OpenStream(&new_stream,
	                    &input_params,
	                    NULL,                                   /* Input-only stream */
	                    samplerate,                             /* Agreed upon samplerate */
	                    paFramesPerBufferUnspecified,           /* Let portaudio choose the best size */
	                    paNoFlag | paDitherOff | paClipOff,     /* Disable extra audio processing */
	                    NULL,
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

	_state.device_count = 0;
	free(_state.device_names);
	free(_state.device_idxs);

	return 0;
}

int
audio_read(float *ptr, size_t count)
{
	/* If stream is not yet open, exit */
	if (!_state.stream || Pa_IsStreamStopped(_state.stream)) {
		return 1;
	}

	Pa_ReadStream(_state.stream, ptr, count);
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
static PaError
print_error(PaError err)
{
	fprintf(stderr, "Portaudio error: %s\n", Pa_GetErrorText(err));
	return err;
}

static int
audio_stop_stream(void)
{
	if (!_state.stream) return 0;

	/* Stop streaming audio and deinitialize stream */
	Pa_CloseStream(&_state.stream);
	_state.stream = NULL;

	return 0;
}

/* }}} */
