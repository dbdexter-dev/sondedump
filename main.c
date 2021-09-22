#include <getopt.h>
#include <stdio.h>
#include "decode/rs41/rs41.h"
#include "wavfile.h"

static int read_wrapper(float *dst);
static FILE *_wav;
static int _bps;

int
main(int argc, char *argv[])
{
	int samplerate;
	RS41Decoder rs41decoder;

	if (!(_wav = fopen(argv[1], "rb"))) {
		fprintf(stderr, "Could not open input file\n");
		return 1;
	}

	if (wav_parse(_wav, &samplerate, &_bps)) {
		fprintf(stderr, "Could not recognize input file type\n");
		return 2;
	}

	rs41_decoder_init(&rs41decoder, samplerate);
	while (rs41_decode(&rs41decoder, &read_wrapper))
		;


	rs41_decoder_deinit(&rs41decoder);
	fclose(_wav);

	return 0;
}

static int
read_wrapper(float *dst)
{
	return wav_read(dst, _bps, _wav);
}
