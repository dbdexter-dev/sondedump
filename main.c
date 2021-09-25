#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include "decode/common.h"
#include "decode/rs41/rs41.h"
#include "gps/ecef.h"
#include "wavfile.h"

#define SEPARATOR "     "

static int wav_read_wrapper(float *dst);
static int raw_read_wrapper(float *dst);
static FILE *_wav;
static int _bps;

static void printf_packet(SondeData *data);

int
main(int argc, char *argv[])
{
	int samplerate;
	int (*read_wrapper)(float *dst);
	RS41Decoder rs41decoder;
	SondeData data;

	if (!(_wav = fopen(argv[1], "rb"))) {
		fprintf(stderr, "Could not open input file\n");
		return 1;
	}

	read_wrapper = &wav_read_wrapper;
	if (wav_parse(_wav, &samplerate, &_bps)) {
		fprintf(stderr, "Could not recognize input file type\n");
		fprintf(stderr, "Will assume raw, mono, 16 bit, 48kHz\n");
		samplerate = 48000;

		read_wrapper = &raw_read_wrapper;
	}

	rs41_decoder_init(&rs41decoder, samplerate);
	while (1) {
		data = rs41_decode(&rs41decoder, read_wrapper);
		if (data.type == SOURCE_END) break;
		printf_packet(&data);
	}

	rs41_decoder_deinit(&rs41decoder);
	fclose(_wav);

	return 0;
}

static int
wav_read_wrapper(float *dst)
{
	return wav_read(dst, _bps, _wav);
}

static int
raw_read_wrapper(float *dst)
{
	int16_t tmp;

	if (!fread(&tmp, 2, 1, _wav)) return 0;;

	*dst = tmp;
	return 1;
}

static void
printf_packet(SondeData *data)
{
	float lat, lon, alt, spd, hdg, climb;
	static int newline = 0;

	switch (data->type) {
		case EMPTY:
			break;
		case FRAME_END:
			if (newline) printf("\n");
			newline = 0;
			return;
		case UNKNOWN:
			break;
		case INFO:
			printf("[%5d] ", data->data.info.seq);
			if (data->data.info.burstkill_status > 0) {
				printf("Burstkill: %d:%02d:%02d ",
						data->data.info.burstkill_status/3600,
						data->data.info.burstkill_status/60%60,
						data->data.info.burstkill_status%60
					  );
			}
			printf(SEPARATOR);
			break;
		case PTU:
			printf("%5.1f'C %3.0f%% rh",
					data->data.ptu.temp,
					data->data.ptu.rh
				  );
			if (data->data.ptu.pressure > 0) {
				printf(" %.0f mB", data->data.ptu.pressure);
			}
			printf(SEPARATOR);
			break;
		case POSITION:
			ecef_to_lla(&lat, &lon, &alt, data->data.pos.x, data->data.pos.y, data->data.pos.z);
			ecef_to_spd_hdg(&spd, &hdg, &climb, lat, lon, data->data.pos.dx, data->data.pos.dy, data->data.pos.dz);

			printf("%7.4f%s %7.4f%s %5.0fm  Spd: %5.1fm/s Hdg: %3.0f' Climb: %+5.1fm/s",
					fabs(lat), lat >= 0 ? "N" : "S",
					fabs(lon), lon >= 0 ? "E" : "W",
					alt, spd, hdg, climb
				  );
			printf(SEPARATOR);
			break;

		case SOURCE_END:
			break;

	}
	newline = 1;
}
