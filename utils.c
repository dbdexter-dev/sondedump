#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "utils.h"

void
bitcpy(uint8_t *dst, const uint8_t *src, size_t offset, size_t bits)
{
	src += offset / 8;
	offset %= 8;

	/* All but last reads */
	for (; bits > 8; bits -= 8) {
		*dst    = *src++ << offset;
		*dst++ |= *src >> (8 - offset);
	}

	/* Last read */
	if (offset + bits < 8) {
		*dst = (*src << offset) & ~((1 << (8 - bits)) - 1);
	} else {
		*dst  = *src++ << offset;
		*dst |= *src >> (8 - offset);
		*dst &= ~((1 << (8-bits)) - 1);
	}
}

uint64_t
bitmerge(uint8_t *data, int nbits)
{
	uint64_t ret = 0;

	for (; nbits >= 8; nbits-=8) {
		ret = (ret << 8) | *data++;
	}

	return (ret << nbits) | (*data >> (7 - nbits));
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
	const float g0 = 9.80665;
	const float M = 0.0289644;
	const float R_star = 8.3144598;

	const float hbs[] = {0,       11000,   20000,   32000,  47000,  51000,  77000};
	const float Lbs[] = {-0.0065, 0,       0.001,   0.0028, 0.0,   -0.0028, -0.002};
	const float Pbs[] = {101325,  22632.1, 5474.89, 868.02, 110.91, 66.94,  3.96};
	const float Tbs[] = {288.15,  216.65,  216.65,  228.65, 270.65, 270.65, 214.65};

	float Lb, Pb, Tb, hb;
	int b;

	for (b=0; b<(int)LEN(Lbs)-1; b++) {
		if (alt < hbs[b+1]) {
			Lb = Lbs[b];
			Pb = Pbs[b];
			Tb = Tbs[b];
			hb = hbs[b];
			break;
		}
	}

	if (b == (int)LEN(Lbs) - 1) {
		Lb = Lbs[b];
		Pb = Pbs[b];
		Tb = Tbs[b];
		hb = hbs[b];
	}

	if (Lb != 0) {
		return 1e-2 * Pb * powf((Tb + Lb * (alt - hb)) / Tb, - (g0 * M) / (R_star * Lb));
	}
	return 1e-2 * Pb * expf(-g0 * M * (alt - hb) / (R_star * Tb));
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
	return 237.3f * tmp  / (1 - tmp);
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
			"   -t, --type <type>       Enable decoder for the given sonde type\n"
			"                           Supported values: rs41, dfm, m10\n"

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
#ifdef ENABLE_TUI
			" +ncurses"
#endif
			"\n");
}
