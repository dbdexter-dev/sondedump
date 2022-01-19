#include <portaudio.h>
#include <stdio.h>
#include <string.h>
#include "audio.h"
#include "utils.h"

static PaError print_error(PaError err);

static const double _samplerates[] = {48000, 44100};
static struct {
	PaStream *stream;
	float buffer[BUFFER_SIZE];
	unsigned long idx;
} _state;

int
audio_init(int device_num)
{
	int i;
	int num_devices;
	double samplerate;
	const PaDeviceInfo *device_info;
	PaStreamParameters input_params = {
		.device = 0,
		.channelCount = 1,
		.sampleFormat = paFloat32,
		.suggestedLatency = 0,
		.hostApiSpecificStreamInfo = NULL
	};
	PaError err;

	_state.idx = 0;

	/* Initialize portaudio */
	if ((err = Pa_Initialize()) != paNoError) {
		return print_error(err);
	}

	/* Query devices */
	if ((num_devices = Pa_GetDeviceCount()) < 0) {
		fprintf(stderr, "[ERROR] Pa_GetDeviceCount returned %d\n", num_devices);
		return num_devices;
	}

	while (device_num < 0 || device_num >= num_devices) {
		/* List all devices */
		printf("\n==============================\n");
		printf("Please select an audio device:\n");
		for (i=0; i<num_devices; i++) {
			device_info = Pa_GetDeviceInfo(i);
			if (device_info->maxInputChannels > 0) {
				printf("%d) %s\n", i, device_info->name);
			}
		}

		/* Interactively ask for device number */
		printf("Device index: ");
		scanf("%d", &device_num);
	}

	/* Attempt to open the device as a mono input, zero channel output. Try a
	 * bunch of different, known working sample rates */
	device_info = Pa_GetDeviceInfo(device_num);
	input_params.device = device_num;
	input_params.suggestedLatency = device_info->defaultHighInputLatency;

	samplerate = -1;
	for (i=0; i<(int)LEN(_samplerates); i++) {
		if (Pa_IsFormatSupported(&input_params, NULL, _samplerates[i]) == paFormatIsSupported) {
			samplerate = _samplerates[i];
			break;
		}
	}
	if (samplerate < 0) {
		fprintf(stderr, "[ERROR] Could not initialize specified device with sane parameters\n");
		return -1;
	}


	/* Open stream */
	err = Pa_OpenStream(&_state.stream,
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
	if ((err = Pa_StartStream(_state.stream)) != paNoError) {
		return print_error(err);
	}


	return (int)samplerate;
}

int
audio_deinit(void)
{
	/* Stop streaming audio, deinitialize stream, then deinitialize portaudio
	 * itself. If we error here nobody cares cause we're closing shop anyway */
	Pa_AbortStream(&_state.stream);
	Pa_CloseStream(&_state.stream);
	Pa_Terminate();

	return 0;
}

int
audio_read(float *ptr, size_t count)
{
	Pa_ReadStream(_state.stream, ptr, count);

	return 1;
}

/* Static functions {{{ */
static PaError
print_error(PaError err)
{
	fprintf(stderr, "Portaudio error: %s\n", Pa_GetErrorText(err));
	return err;
}
/* }}} */
