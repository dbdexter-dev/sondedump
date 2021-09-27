#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "decode/common.h"
#include "decode/rs41/rs41.h"
#include "gps/time.h"
#include "gps/ecef.h"
#include "io/gpx.h"
#include "io/kml.h"
#include "io/wavfile.h"

#define SEPARATOR "     "
#define SHORTOPTS "f:gh:k:l:o:v"

typedef struct {
	int seq;
	float lat, lon, alt;
	float speed, heading, climb;
	float temp, rh, pressure;
	time_t utc_time;
	char serial[8+1];
} PrintableData;

static void fill_printable_data(PrintableData *to_print, SondeData *data);
static int printf_data(const char *fmt, PrintableData *data);
static int wav_read_wrapper(float *dst);
static int raw_read_wrapper(float *dst);

static FILE *_wav;
static int _bps;
static struct option longopts[] = {
	{ "format",   1, NULL, 'f' },
	{ "gpx",      1, NULL, 'g' },
	{ "help",     0, NULL, 'h' },
	{ "kml",      1, NULL, 'k' },
	{ "live-kml", 1, NULL, 'l' },
	{ "output",   1, NULL, 'o' },
	{ "version",  0, NULL, 'v' },
};


int
main(int argc, char *argv[])
{
	PrintableData printable;
	RS41Decoder rs41decoder;
	SondeData data;
	KMLFile kml, live_kml;
	GPXFile gpx;
	int samplerate;
	int (*read_wrapper)(float *dst);
	int c;
	int has_data;

	/* Command-line changeable parameters {{{ */
	char *output_fmt = "[f]\tt r\tl o a\ts h c";
	char *live_kml_fname = NULL;
	char *kml_fname = NULL;
	char *gpx_fname = NULL;
	char *input_fname;
	/* }}} */
	/* Parse command-line args {{{ */
	while ((c = getopt_long(argc, argv, SHORTOPTS, longopts, NULL)) != -1) {
		switch (c) {
			case 'g':
				gpx_fname = optarg;
				break;
			case 'k':
				kml_fname = optarg;
				break;
			case 'l':
				live_kml_fname = optarg;
				break;
			case 'h':
				usage(argv[0]);
				return 0;
			case 'v':
				version();
				return 0;
			case 'f':
				output_fmt = optarg;
				break;
			default:
				usage(argv[0]);
				return 1;

		}
	}

	if (argc - optind < 1) {
		fprintf(stderr, "No input file specified\n");
		usage(argv[0]);
		return 1;
	}
	input_fname = argv[optind];
	/* }}} */


	if (!(_wav = fopen(input_fname, "rb"))) {
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

	if (kml_fname && kml_init(&kml, kml_fname, 0)) {
		fprintf(stderr, "Error creating KML file %s\n", kml_fname);
		return 1;
	}
	if (live_kml_fname && kml_init(&live_kml, live_kml_fname, 1)) {
		fprintf(stderr, "Error creating KML file %s\n", live_kml_fname);
		return 1;
	}
	if (gpx_fname && gpx_init(&gpx, gpx_fname)) {
		fprintf(stderr, "Error creating GPX file %s\n", gpx_fname);
		return 1;
	}


	rs41_decoder_init(&rs41decoder, samplerate);
	has_data = 0;
	while (1) {
		data = rs41_decode(&rs41decoder, read_wrapper);
		fill_printable_data(&printable, &data);

		if (data.type == SOURCE_END) break;
		switch (data.type) {
			case FRAME_END:
				/* Write line at the end of the frame */
				if (has_data) {
					printf_data(output_fmt, &printable);
					has_data = 0;
				}
				break;
			case POSITION:
				/* Add position to whichever files are open */

				/* Add point to GPS track */
				if (kml_fname) {
					if (!kml.track_active) kml_start_track(&kml, printable.serial);
					kml_add_trackpoint(&kml, printable.lat, printable.lon, printable.alt, printable.utc_time);
				}
				if (live_kml_fname) {
					if (!live_kml.track_active) kml_start_track(&live_kml, printable.serial);
					kml_add_trackpoint(&live_kml, printable.lat, printable.lon, printable.alt, printable.utc_time);
				}
				if (gpx_fname) {
					if (!gpx.track_active) gpx_start_track(&gpx, printable.serial);
					gpx_add_trackpoint(&gpx, printable.lat, printable.lon, printable.alt, printable.utc_time);
				}
				has_data = 1;
				break;
			default:
				has_data = 1;
				break;
		}
	}
	if (kml_fname) kml_close(&kml);
	if (live_kml_fname) kml_close(&live_kml);
	if (gpx_fname) gpx_close(&gpx);

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
fill_printable_data(PrintableData *to_print, SondeData *data)
{
	float lat, lon, alt, spd, hdg, climb;

	switch (data->type) {
		case EMPTY:
		case FRAME_END:
		case UNKNOWN:
		case SOURCE_END:
			break;
		case DATETIME:
			to_print->utc_time = data->data.datetime.datetime;
			break;
		case INFO:
			strncpy(to_print->serial, data->data.info.sonde_serial, LEN(to_print->serial)-1);
			to_print->serial[8] = 0;
			to_print->seq = data->data.info.seq;
			/*
			if (data->data.info.burstkill_status > 0) {
				printf("Burstkill: %d:%02d:%02d ",
						data->data.info.burstkill_status/3600,
						data->data.info.burstkill_status/60%60,
						data->data.info.burstkill_status%60
					  );
			}
			*/
			break;
		case PTU:
			to_print->temp = data->data.ptu.temp;
			to_print->rh = data->data.ptu.rh;
			to_print->pressure  = data->data.ptu.rh;
			break;
		case POSITION:
			ecef_to_lla(&lat, &lon, &alt, data->data.pos.x, data->data.pos.y, data->data.pos.z);
			ecef_to_spd_hdg(&spd, &hdg, &climb, lat, lon, data->data.pos.dx, data->data.pos.dy, data->data.pos.dz);

			to_print->lat = lat;
			to_print->lon = lon;
			to_print->alt = alt;
			to_print->speed = spd;
			to_print->heading = hdg;
			to_print->climb = climb;
			break;
	}
}

static int
printf_data(const char *fmt, PrintableData *data)
{
	size_t i;
	int is_literal;
	char time[64];


	is_literal = 0;
	for (i=0; i<strlen(fmt); i++) {
		if (is_literal) {
			putchar(fmt[i]);
		} else {
			switch (fmt[i]) {
				case '\\':
					is_literal = 1;
					break;
				case 'a':
					printf("%6.0fm", data->alt);
					break;
				case 'c':
					printf("%+5.1fm/s", data->climb);
					break;
				case 'd':
					strftime(time, LEN(time), "%a %b %d %Y %H:%M:%S", gmtime(&data->utc_time));
					printf("%s", time);
					break;
				case 'f':
					printf("%5d", data->seq);
					break;
				case 'h':
					printf("%3.0f'", data->heading);
					break;
				case 'l':
					printf("%7.4f%c", data->lat, (data->lat >= 0 ? 'N' : 'S'));
					break;
				case 'o':
					printf("%7.4f%c", data->lon, (data->lon >= 0 ? 'E' : 'W'));
					break;
				case 'p':
					printf("%4.0f hPa", data->pressure);
					break;
				case 'r':
					printf("%3.0f%%", data->rh);
					break;
				case 's':
					printf("%4.1fm/s", data->speed);
					break;
				case 'S':
					printf("%s", data->serial);
					break;
				case 't':
					printf("%5.1f'C", data->temp);
					break;
				default:
					putchar(fmt[i]);
					break;
			}
		}
		is_literal = 0;
	}
	printf("\n");
	return 0;
}
