#include <getopt.h>
#include <stdio.h>
#include "demod/gfsk.h"
#include "wavfile.h"

static int read_wrapper(float *dst, size_t len);
static FILE *_wav;
static int _bps;

int
main(int argc, char *argv[])
{
	uint8_t data[128];
	int samplerate;

	if (!(_wav = fopen(argv[1], "rb"))) {
		fprintf(stderr, "Could not open input file\n");
		return 1;
	}

	if (wav_parse(_wav, &samplerate, &_bps)) {
		fprintf(stderr, "Could not recognize input file type\n");
		return 2;
	}


	gfsk_init(samplerate, 4800);
	FILE *out = fopen("/tmp/gfsk.data", "wb");

	while (gfsk_decode(data, 0, 128*8, &read_wrapper)) {
		printf(".");
		fflush(stdout);
		fwrite(data, 128, 1, out);
	}

	fclose(out);
	gfsk_deinit();
	fclose(_wav);

	return 0;
}

static int
read_wrapper(float *dst, size_t len)
{
	return wav_read(dst, _bps, _wav);
}
