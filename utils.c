#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "utils.h"

static char _generated_fname[sizeof("LRPT_YYYY_MM_DD_HH_MM.s") + 1];

void
bitcpy(uint8_t *dst, uint8_t *src, size_t src_offset, size_t num_bits)
{
	const uint8_t src_mask = (1 << (8 - src_offset)) - 1;
	size_t i;
	int left;
	uint8_t byte;

	src += src_offset / 8;
	src_offset %= 8;

	/* First few bits */
	byte = (src[0] & src_mask) << src_offset;

	/* Main loop: one byte at a time */
	for (i=1; i<num_bits/8; i++) {
		byte |= (src[i] >> (8 - src_offset));
		dst[i-1] = byte;
		byte = (src[i] & src_mask) << src_offset;
	}

	/* Last few bits */
	left = num_bits - (i*8 - src_offset);
	if (left > 0) {
		byte |= src[i] >> (8 - src_offset);
		byte &= ~((1 << left) - 1);
		dst[i-1] = byte;
	}
}

char
*my_strdup(char *str)
{
	size_t len;
	char *ret;

	len = strlen(str) + 1;
	ret = malloc(len);
	if (ret) {
		memcpy(ret, str, len);
	}

	return ret;
}

float
altitude_to_pressure(float alt)
{
	return 1013.25f * powf(1.0f - 2.25577f * 1e-5f * alt, 5.25588f);
}

float
pressure_to_altitude(float pressure)
{
	return 44330 * (1 - powf((pressure / 1013.25f), 1/5.25588f));
}

float
dewpt(float temp, float rh)
{
	const float tmp = (logf(rh / 100.0f) + (17.27f * temp / (237.3f + temp))) / 17.27f;
	return 237.3f * tmp  / (1 + tmp);
}

float
sat_mixing_ratio(float temp, float p)
{
	const float wv_pressure = 610.97e-3 * expf((17.625*temp)/(temp+243.04));
	const float wice_pressure = 611.21e-3 * expf((22.587*temp)/(temp+273.86));

	if (temp < 0) {
		return 621.97 * wice_pressure / (p - wice_pressure);
	} else {
		return 621.97 * wv_pressure / (p - wv_pressure);
	}
}

void
usage(const char *pname)
{
	fprintf(stderr, "Usage: %s [options] file_in\n", pname);
	fprintf(stderr,
#ifdef ENABLE_AUDIO
			"   -a, --audio-device <id> Use PortAudio device <id> as input (default: choose interactively)\n"
#endif
			"   -c, --csv <file>        Output data to <file> in CSV format\n"
			"   -f, --fmt <format>      Format output lines as <format>\n"
			"   -g, --gpx <file>        Output GPX track to <file>\n"
			"   -k, --kml <file>        Output KML track to <file>\n"
			"   -l, --live-kml <file>   Output live KML track to <file>\n"
#ifdef ENABLE_DIAGRAMS
			"       --stuve <file>      Generate Stuve diagram and output to <file>\n"
#endif

	        "\n"
	        "   -h, --help              Print this help screen\n"
	        "   -v, --version           Print version info\n"
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
}

void
version()
{
	fprintf(stderr,
			"sondedump v" VERSION
#ifdef ENABLE_AUDIO
			" +portaudio"
#endif
#ifdef ENABLE_DIAGRAMS
			" +cairo"
#endif
#ifdef ENABLE_TUI
			" +ncurses"
#endif
			"\n");
}
