#ifdef _WIN32
#include "win_getopt.h"
#else
#include <getopt.h>
#endif
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "decode.h"
#include "gps/ecef.h"
#include "gps/time.h"
#include "io/gpx.h"
#include "io/kml.h"
#include "io/wavfile.h"
#include "log/log.h"
#include "physics.h"
#include "utils.h"
#ifdef ENABLE_TUI
#include "tui/tui.h"
#endif
#ifdef ENABLE_GUI
#include "gui/gui.h"
#endif
#ifdef ENABLE_AUDIO
#include "io/audio.h"
#endif

#define BUFLEN 1024

#define SHORTOPTS "a:c:f:g:hk:l:o:r:t:v"

enum ui {
	UI_TEXT,
	UI_TUI,
	UI_GUI
};

static void usage(const char *progname);
static void version(void);
static int printf_data(const char *fmt, PrintableData *data);
static int wav_read_wrapper(float *dst, size_t count);
static int raw_read_wrapper(float *dst, size_t count);
static void sigint_handler(int val);
static int ascii_to_decoder(const char *ascii);

#ifdef ENABLE_AUDIO
static int audio_read_wrapper(float *dst, size_t count);
#endif

volatile int _interrupted;

static FILE *_wav;
static int _bps;

const char *_decoder_names[] = {"Auto", "DFM", "iMet-4", "iMS-100", "M10/M20", "RS41"};
const char *_decoder_argvs[] = {"auto", "dfm", "imet4", "ims100", "m10", "rs41"};
const int _decoder_count = LEN(_decoder_names);

static struct option longopts[] = {
#ifdef ENABLE_AUDIO
	{ "audio-device", 1, NULL, 'a' },
#endif
	{ "fmt",          1, NULL, 'f' },
	{ "csv",          1, NULL, 'c' },
	{ "decoders",     1, NULL, 'd' },
	{ "gpx",          1, NULL, 'g' },
	{ "help",         0, NULL, 'h' },
	{ "kml",          1, NULL, 'k' },
	{ "live-kml",     1, NULL, 'l' },
	{ "output",       1, NULL, 'o' },
	{ "location",     1, NULL, 'r' },
	{ "type",         1, NULL, 't' },
	{ "version",      0, NULL, 'v' },
#ifdef ENABLE_GUI
	{ "gui",          0, NULL, 'u' },
#endif
	{ NULL,           0, NULL,  0  }
};


int
main(int argc, char *argv[])
{
	PrintableData *printable;
	KMLFile kml, live_kml;
	GPXFile gpx;
	int samplerate;
	int (*read_wrapper)(float *dst, size_t count);

	int c;
	int slot = -1;
	FILE *csv_fd = NULL;

	float srcbuf[BUFLEN];

	/* Command-line changeable parameters {{{ */
	char *output_fmt = "[%f] %t'C %r%%    %l %o %am    %sm/s %h' %cm/s";
	const char *live_kml_fname = NULL;
	const char *kml_fname = NULL;
	const char *gpx_fname = NULL;
	const char *csv_fname = NULL;
	const char *input_fname = NULL;
	enum ui ui = UI_TEXT;
	int receiver_location_set = 0;
	enum decoder active_decoder = AUTO;
	float receiver_lat = 0, receiver_lon = 0, receiver_alt = 0;
#ifdef ENABLE_TUI
	ui = UI_TUI;
#endif
#ifdef ENABLE_AUDIO
	int input_from_audio = 0;
	int audio_device = -1;
	int audio_device_count = 0;
	const char **audio_device_names;
	int i;
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
			case 't':
				active_decoder = ascii_to_decoder(optarg);
				if (active_decoder < 0) {
					log_error("Unsupported type: %s", optarg);
					usage(argv[0]);
					return 1;
				}
				break;
			case 'r':
				if (sscanf(optarg, "%f,%f,%f", &receiver_lat, &receiver_lon, &receiver_alt) < 3) {
					log_error("Invalid receiver coordinates");
					usage(argv[0]);
					return 1;
				}
				receiver_location_set = 1;
				break;
			case 'f':
				output_fmt = optarg;
				ui = UI_TEXT;
				break;
			case 'h':
				usage(argv[0]);
				return 0;
			case 'v':
				version();
				return 0;
#ifdef ENABLE_GUI
			case 'u':
				ui = UI_GUI;
				break;
#endif
			default:
				usage(argv[0]);
				return 1;

		}
	}

	if (argc - optind < 1) {
#ifdef ENABLE_AUDIO
		/* Initialize audio backend */
		if (audio_init()) {
			log_error("Error while initializing audio subsystem, exiting...");
			return 1;
		}

		input_from_audio = 1;

		switch (ui) {
			case UI_TEXT:
			case UI_TUI:
				/* Ask the user for a device now */
				audio_device_count = audio_get_num_devices();
				audio_device_names = audio_get_device_names();
				samplerate = -1;

				/* If the audio device was not specified as a command-line arg */
				if (audio_device < 0) {
					/* Ask user dyamically for an audio device */
					printf("\n==============================\n");
					printf("Please select an audio device:\n");
					for (i = 0; i < audio_device_count; i++) {
						if (audio_device_names[i]) {
							printf("%d) %s\n", i, audio_device_names[i]);
						}
					}

					printf("Device index: ");
					scanf("%d", &audio_device);
				}

				/* Open device */
				samplerate = audio_open_device(audio_device);
				if (samplerate < 0) {
					return -1;
				}
				printf("Selected samplerate: %d\n", samplerate);
				break;
			case UI_GUI:
				/* Open first device. If open fails, defer device opening to the GUI */
				samplerate = audio_open_device(0);
				if (samplerate < 1) samplerate = 48000;
				break;
		}
#else
		log_error("No input file specified");
		usage(argv[0]);
		return 1;
#endif
	}
	input_fname = argv[optind];
	/* }}} */

	/* Open input */
#ifdef ENABLE_AUDIO
	if (input_from_audio) {
		read_wrapper = audio_read_wrapper;
	} else {
#endif
	if (!(_wav = fopen(input_fname, "rb"))) {
		log_error("Could not open input file");
		return 1;
	}
	read_wrapper = wav_read_wrapper;
	if (wav_parse(_wav, &samplerate, &_bps)) {
		log_warn("Could not recognize input file type");
		log_warn("Will assume raw, mono, 32 bit float, 48kHz");
		samplerate = 48000;

		read_wrapper = &raw_read_wrapper;
	}
#ifdef ENABLE_AUDIO
	}   /* if (input_from_audio) {} else { */
#endif

	/* Open CSV output */
	if (csv_fname) {
		if (!(csv_fd = fopen(csv_fname, "wb"))) {
			log_error("Error creating CSV file %s", csv_fname);
		}
		fprintf(csv_fd, "Temperature,RH,Pressure,Altitude,Latitude,Longitude,Speed,Heading,Climb\n");
	}

	/* Open GPX/KML output */
	if (kml_fname && kml_init(&kml, kml_fname, 0)) {
		log_error("Error creating KML file %s", kml_fname);
		return 1;
	}
	if (live_kml_fname && kml_init(&live_kml, live_kml_fname, 1)) {
		log_error("Error creating KML file %s", live_kml_fname);
		return 1;
	}
	if (gpx_fname && gpx_init(&gpx, gpx_fname)) {
		log_error("Error creating GPX file %s", gpx_fname);
		return 1;
	}

	/* Initialize decoder */
	set_active_decoder(active_decoder);
	if (decoder_init(samplerate)) {
		log_error("Error while initializing decoder subsystem");
		return 1;
	}

#ifdef ENABLE_TUI
	/* Enable TUI */
	if (ui == UI_TUI) {
		tui_init(-1);
		if (receiver_location_set) {
			tui_set_ground_location(receiver_lat, receiver_lon, receiver_alt);
		}
	}
#endif
#ifdef ENABLE_GUI
	if (ui == UI_GUI) {
		gui_init();
	}
#endif

	/* Catch SIGINT to exit the loop */
	_interrupted = 0;
	signal(SIGINT, sigint_handler);

	/* Initialize srcbuf just in case the audio device init failed */
	memset(srcbuf, 0, sizeof(srcbuf));

	/* Process decoded frames */
	while (!_interrupted) {
		/* Read new samples */
		if (read_wrapper(srcbuf, LEN(srcbuf)) <= 0) break;

		/* Send them to decoder */
		while (decode(srcbuf, LEN(srcbuf)) != PROCEED) {
#ifdef ENABLE_GUI
			if (ui == UI_GUI) {
				gui_force_update();
			}
#endif
			/* If no new data, immediately go to next iteration */
			if (slot == get_slot()) {
				continue;
			}
			slot = get_slot();

			printable = get_data();
			if (ui == UI_TEXT) {
				/* Print new data */
				printf_data(output_fmt, printable);
			}

			/* If we are also calibrated enough, and CSV output is enabled, append
			 * the new datapoint to the file */
			if (csv_fd && printable->calibrated) {
				fprintf(csv_fd, "%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
						printable->temp, printable->rh, printable->pressure,
						printable->alt, printable->lat, printable->lon,
						printable->speed, printable->heading, printable->climb);
			}

			/* Add position to whichever files are open */
			if (kml_fname) {
				kml_start_track(&kml, printable->serial);
				kml_add_trackpoint(&kml, printable->lat, printable->lon, printable->alt);
			}
			if (live_kml_fname) {
				kml_start_track(&live_kml, printable->serial);
				kml_add_trackpoint(&live_kml, printable->lat, printable->lon, printable->alt);
			}
			if (gpx_fname) {
				gpx_start_track(&gpx, printable->serial);
				gpx_add_trackpoint(&gpx,
						printable->lat, printable->lon, printable->alt,
						printable->speed, printable->heading, printable->utc_time);
			}
		}
	}

	/* Close all open files */
	if (kml_fname) kml_close(&kml);
	if (live_kml_fname) kml_close(&live_kml);
	if (gpx_fname) gpx_close(&gpx);
	if (csv_fd) fclose(csv_fd);

	if (_wav) fclose(_wav);
#ifdef ENABLE_AUDIO
	if (input_from_audio) {
		audio_deinit();
	}
#endif
#ifdef ENABLE_TUI
	if (ui == UI_TUI) {
		tui_deinit();
	}
#endif
#ifdef ENABLE_GUI
	if (ui == UI_GUI) {
		gui_deinit();
	}
#endif
	/* Deinit decoder */
	decoder_deinit();

	return 0;
}

/* Static functions {{{ */
static int
wav_read_wrapper(float *dst, size_t count)
{
	if (_interrupted) return 0;
	return wav_read(dst, _bps, count, _wav);
}

static int
raw_read_wrapper(float *dst, size_t count)
{
	if (_interrupted) return 0;
	if (!fread(dst, 4, count, _wav)) return 0;
	return 1;
}

#ifdef ENABLE_AUDIO
static int
audio_read_wrapper(float *dst, size_t count)
{
	if (_interrupted) return 0;
	return audio_read(dst, count);
}
#endif


static void
usage(const char *pname)
{
	fprintf(stderr, "Usage: %s [options] file_in\n", pname);
	fprintf(stderr,
#ifdef ENABLE_AUDIO
			"   -a, --audio-device <id> Use PortAudio device <id> as input (default: choose interactively)\n"
#endif
			"   -c, --csv <file>             Output data to <file> in CSV format\n"
			"   -f, --fmt <format>           Format output lines as <format>\n"
			"   -g, --gpx <file>             Output GPX track to <file>\n"
			"   -k, --kml <file>             Output KML track to <file>\n"
			"   -l, --live-kml <file>        Output live KML track to <file>\n"
			"   -r, --location <lat,lon,alt> Set receiver location to <lat, lon, alt> (default: none)\n"
			"   -t, --type <type>            Enable decoder for the given sonde type. Supported values:\n"
			"                                auto: Autodetect\n"
			"                                rs41: Vaisala RS41-SG(P,M)\n"
			"                                dfm: GRAW DFM06/09\n"
			"                                m10: MeteoModem M10/M20\n"
			"                                ims100: Meisei iMS-100\n"
			"                                imet4: InterMet iMet-4\n"
#ifdef ENABLE_GUI
			"   -g, --gui                    Initialize GUI\n"
#endif
	        "\n"
	        "   -h, --help                   Print this help screen\n"
	        "   -v, --version                Print version info\n"
	        );
	fprintf(stderr,
			"\nAvailable format specifiers:\n"
			"   %%a      Altitude (m)\n"
			"   %%b      Burstkill/shutdown timer\n"
			"   %%c      Climb rate (m/s)\n"
			"   %%d      Dew point (degrees Celsius)\n"
			"   %%f      Frame counter\n"
			"   %%h      Heading (degrees)\n"
			"   %%l      Latitude (decimal degrees + N/S)\n"
			"   %%o      Longitude (decimal degrees + E/W)\n"
			"   %%p      Pressure (hPa)\n"
			"   %%r      Relative humidity (%%)\n"
			"   %%s      Speed (m/s)\n"
			"   %%S      Sonde serial number\n"
			"   %%t      Temperature (degrees Celsius)\n"
			"   %%T      Timestamp (yyyy-mm-dd hh:mm::ss, local)\n"
		   );
#ifdef ENABLE_TUI
	fprintf(stderr,
	        "\nTUI keybinds:\n"
	        "   Arrow keys: change active decoder\n"
	        "   Tab: toggle between absolute (lat, lon, alt) and relative (az, el, range) coordinates (requires -r, --location)\n"
	       );
#endif
}

static void
version(void)
{
	fprintf(stderr,
			"sondedump v" VERSION
#ifdef ENABLE_AUDIO
			" +portaudio"
#endif
#ifdef ENABLE_TUI
			" +ncurses"
#endif
#ifdef ENABLE_GUI
			" +gui"
#endif
			"\n");
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
					printf("%4.1f", isnormal(data->pressure) ? data->pressure : altitude_to_pressure(data->alt) );
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
#ifdef ENABLE_GUI
	gui_deinit();
#endif
}

static int
ascii_to_decoder(const char *ascii)
{
	size_t i;

	for (i=0; i<LEN(_decoder_argvs); i++) {
		if (!strcasecmp(ascii, _decoder_argvs[i])) return i;
	}
	return -1;
}
/* }}} */
