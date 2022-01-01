#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "decode/common.h"
#include "decode/rs41/rs41.h"
#include "decode/dfm09/dfm09.h"
#include "decode/m10/m10.h"
#include "decode/rs92/rs92.h"
#include "gps/ecef.h"
#include "gps/time.h"
#include "io/gpx.h"
#include "io/kml.h"
#include "io/wavfile.h"
#include "utils.h"
#ifdef ENABLE_TUI
#include "tui/tui.h"
#endif
#ifdef ENABLE_AUDIO
#include "io/audio.h"
#endif

#define SHORTOPTS "a:c:f:g:hk:l:o:t:v"

typedef struct {
	int seq;
	float lat, lon, alt;
	float speed, heading, climb;
	float temp, rh, pressure;
	time_t utc_time;
	int shutdown_timer;
	char serial[32];
	char xdata[128];
} PrintableData;

static void fill_printable_data(PrintableData *to_print, SondeData *data);
static int printf_data(const char *fmt, PrintableData *data);
static int wav_read_wrapper(float *dst);
static int raw_read_wrapper(float *dst);
static void sigint_handler(int val);

#ifdef ENABLE_TUI
static void decoder_changer(int delta);
#endif

static FILE *_wav;
static int _bps;
static int _interrupted;
static enum { RS41=0, RS92=1, DFM=2, M10=3, END } _active_decoder;
static struct option longopts[] = {
	{ "audio-device", 1, NULL, 'a'},
	{ "fmt",          1, NULL, 'f' },
	{ "csv",          1, NULL, 'c' },
	{ "decoders",     1, NULL, 'd' },
	{ "gpx",          1, NULL, 'g' },
	{ "help",         0, NULL, 'h' },
	{ "kml",          1, NULL, 'k' },
	{ "live-kml",     1, NULL, 'l' },
	{ "output",       1, NULL, 'o' },
	{ "type",         1, NULL, 't' },
	{ "version",      0, NULL, 'v' },
	{ NULL,           0, NULL,  0  }
};


int
main(int argc, char *argv[])
{
	PrintableData printable;
	SondeData data;
	KMLFile kml, live_kml;
	GPXFile gpx;
	int samplerate;
	int (*read_wrapper)(float *dst);
	int c;
	int has_data;
	FILE *csv_fd = NULL;

	RS41Decoder rs41decoder;
	RS92Decoder rs92decoder;
	DFM09Decoder dfm09decoder;
	M10Decoder m10decoder;

	memset(&printable, 0, sizeof(PrintableData));

	/* Command-line changeable parameters {{{ */
	char *output_fmt = "[%f] %t'C %r%%    %l %o %am    %sm/s %h' %cm/s";
	char *live_kml_fname = NULL;
	char *kml_fname = NULL;
	char *gpx_fname = NULL;
	char *csv_fname = NULL;
	char *input_fname = NULL;
#ifdef ENABLE_TUI
	int tui_enabled = 1;
#endif
#ifdef ENABLE_AUDIO
	int input_from_audio = 0;
	int audio_device = -1;
#endif
	/* }}} */
	/* Parse command-line args {{{ */
	while ((c = getopt_long(argc, argv, SHORTOPTS, longopts, NULL)) != -1) {
		switch (c) {
#ifdef ENABLE_AUDIO
			case 'a':
				audio_device = atoi(optarg);
				break;
#endif
			case 'c':
				csv_fname = optarg;
				break;
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
			case 't':
				if (!strcmp(optarg, "rs41")) {
					_active_decoder = RS41;
				} else if (!strcmp(optarg, "dfm")) {
					_active_decoder = DFM;
				} else if (!strcmp(optarg, "m10")) {
					_active_decoder = M10;
				} else if (!strcmp(optarg, "rs92")) {
					_active_decoder = RS92;
				} else {
					fprintf(stderr, "Unsupported type: %s\n", optarg);
					usage(argv[0]);
					return 1;
				}
				break;
			case 'f':
				output_fmt = optarg;
#ifdef ENABLE_TUI
				tui_enabled = 0;
#endif
				break;
			default:
				usage(argv[0]);
				return 1;

		}
	}

	if (argc - optind < 1) {
#ifdef ENABLE_AUDIO
		samplerate = audio_init(audio_device);
		if (samplerate < 0) return 1;
		printf("Selected samplerate: %d\n", samplerate);
		input_from_audio = 1;
#else
		fprintf(stderr, "No input file specified\n");
		usage(argv[0]);
		return 1;
#endif
	}
	input_fname = argv[optind];
	/* }}} */

	/* Open input */
#ifdef ENABLE_AUDIO
	if (input_from_audio) {
		read_wrapper = audio_read;
	} else {
#endif
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
#ifdef ENABLE_AUDIO
	}   /* if (input_from_audio) {} else { */
#endif

	/* Open CSV output */
	if (csv_fname) {
		if (!(csv_fd = fopen(csv_fname, "wb"))) {
			fprintf(stderr, "Error creating CSV file %s\n", csv_fname);
		}
		fprintf(csv_fd, "Temperature,RH,Pressure,Altitude,Latitude,Longitude,Speed,Heading,Climb\n");
	}

	/* Open GPX/KML output */
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
#ifdef ENABLE_TUI
	if (tui_enabled) {
		tui_init(-1, &decoder_changer);
	}
#endif

	rs41_decoder_init(&rs41decoder, samplerate);
	dfm09_decoder_init(&dfm09decoder, samplerate);
	m10_decoder_init(&m10decoder, samplerate);
	rs92_decoder_init(&rs92decoder, samplerate);

	/* Catch SIGINT to exit the loop */
	_interrupted = 0;
	has_data = 0;
	signal(SIGINT, sigint_handler);
	while (!_interrupted) {
		switch (_active_decoder) {
			case RS41:
				data = rs41_decode(&rs41decoder, read_wrapper);
				break;
			case RS92:
				data = rs92_decode(&rs92decoder, read_wrapper);
				break;
			case DFM:
				data = dfm09_decode(&dfm09decoder, read_wrapper);
				break;
			case M10:
				data = m10_decode(&m10decoder, read_wrapper);
				break;
			default:
				break;
		}

		fill_printable_data(&printable, &data);

		if (data.type == SOURCE_END) break;
#ifdef ENABLE_TUI
		tui_update(&data);
#endif

		switch (data.type) {
			case FRAME_END:
				/* Write line at the end of the frame */
				if (has_data) {
#ifdef ENABLE_TUI
					if (!tui_enabled) {
						printf_data(output_fmt, &printable);
					}
#else
					printf_data(output_fmt, &printable);
#endif
				}

				if (csv_fd) {
					fprintf(csv_fd, "%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
							printable.temp, printable.rh,
							isnan(printable.pressure) ? altitude_to_pressure(printable.alt) : printable.pressure,
							printable.alt, printable.lat, printable.lon,
							printable.speed, printable.heading, printable.climb);
				}

				has_data = 0;
				break;
			case POSITION:
				/* Add position to whichever files are open */
				if (kml_fname) {
					kml_start_track(&kml, printable.serial);
					kml_add_trackpoint(&kml, printable.lat, printable.lon, printable.alt);
				}
				if (live_kml_fname) {
					kml_start_track(&live_kml, printable.serial);
					kml_add_trackpoint(&live_kml, printable.lat, printable.lon, printable.alt);
				}
				if (gpx_fname) {
					gpx_start_track(&gpx, printable.serial);
					gpx_add_trackpoint(&gpx, printable.lat, printable.lon, printable.alt, printable.speed, printable.heading, printable.utc_time);
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
	if (csv_fd) fclose(csv_fd);

	rs41_decoder_deinit(&rs41decoder);
	dfm09_decoder_deinit(&dfm09decoder);
	m10_decoder_deinit(&m10decoder);
	rs92_decoder_deinit(&rs92decoder);
	if (_wav) fclose(_wav);
#ifdef ENABLE_AUDIO
	if (input_from_audio) {
		audio_deinit();
	}
#endif
#ifdef ENABLE_TUI
	if (tui_enabled) {
		tui_deinit();
	}
#endif

	return 0;
}

/* Static functions {{{ */
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
			to_print->seq = data->data.info.seq;
			to_print->shutdown_timer = data->data.info.burstkill_status;
			break;
		case PTU:
			to_print->temp = data->data.ptu.temp;
			to_print->rh = data->data.ptu.rh;
			to_print->pressure  = data->data.ptu.pressure;
			break;
		case POSITION:
			to_print->lat = data->data.pos.lat;
			to_print->lon = data->data.pos.lon;
			to_print->alt = data->data.pos.alt;
			to_print->speed = data->data.pos.speed;
			to_print->heading = data->data.pos.heading;
			to_print->climb = data->data.pos.climb;
			break;
		case XDATA:
			strcpy(to_print->xdata, data->data.xdata.data);
			break;
	}
}

static int
printf_data(const char *fmt, PrintableData *data)
{
	size_t i;
	int escape_seq;
	char time[64];

	escape_seq = 0;
	for (i=0; i<strlen(fmt); i++) {
		if (fmt[i] == '%' && !escape_seq) {
			escape_seq = 1;
		} else if (escape_seq) {
			escape_seq = 0;
			switch (fmt[i]) {
				case 'a':
					printf("%6.0f", data->alt);
					break;
				case 'b':
					if (data->shutdown_timer > 0) {
						printf("%d:%02d:%02d",
								data->shutdown_timer/3600,
								data->shutdown_timer/60%60,
								data->shutdown_timer%60
								);
					} else {
						printf("(disabled)");
					}
					break;
				case 'c':
					printf("%+5.1f", data->climb);
					break;
				case 'd':
					printf("%6.1f", dewpt(data->temp, data->rh));
					break;
				case 'f':
					printf("%5d", data->seq);
					break;
				case 'h':
					printf("%3.0f", data->heading);
					break;
				case 'l':
					printf("%8.5f%c", fabs(data->lat), (data->lat >= 0 ? 'N' : 'S'));
					break;
				case 'o':
					printf("%8.5f%c", fabs(data->lon), (data->lon >= 0 ? 'E' : 'W'));
					break;
				case 'p':
					printf("%4.0f", isnormal(data->pressure) ? data->pressure : altitude_to_pressure(data->alt) );
					break;
				case 'r':
					printf("%3.0f", data->rh);
					break;
				case 's':
					printf("%4.1f", data->speed);
					break;
				case 'S':
					printf("%s", data->serial);
					break;
				case 't':
					printf("%5.1f", data->temp);
					break;
				case 'T':
					strftime(time, LEN(time), "%a %b %d %Y %H:%M:%S", gmtime(&data->utc_time));
					printf("%s", time);
					break;
				default:
					putchar(fmt[i]);
					break;
			}
		} else {
			putchar(fmt[i]);
		}
	}
	if (i > 0) printf("\n");
	return 0;
}

static void
sigint_handler(int val)
{
	(void)val;
	_interrupted = 1;
}

#ifdef ENABLE_TUI
static void
decoder_changer(int delta)
{
	_active_decoder = (_active_decoder + END + delta) % END;
	tui_set_active_decoder(_active_decoder);
}
#endif
/* }}} */
