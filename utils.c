#include <math.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"

static float spline_tangent(const float *xs, const float *ys, int k);
static float hermite_00(float x);
static float hermite_01(float x);
static float hermite_10(float x);
static float hermite_11(float x);

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
bitmerge(const uint8_t *data, int nbits)
{
	uint64_t ret = 0;

	for (; nbits >= 8; nbits-=8) {
		ret = (ret << 8) | *data++;
	}

	return (ret << nbits) | (*data >> (7 - nbits));
}

void
bitpack(uint8_t *dst, const uint8_t *src, int bit_offset, int nbits)
{
	uint8_t tmp;

	dst += bit_offset/8;
	bit_offset %= 8;

	tmp = *dst >> (8 - bit_offset);
	for (; nbits > 0; nbits--) {
		tmp = (tmp << 1) | *src++;
		bit_offset++;

		if (!(bit_offset % 8)) {
			*dst++ = tmp;
			tmp = 0;
		}
	}

	if (bit_offset % 8) {
		*dst &= (1 << (8 - bit_offset%8)) - 1;
		*dst |= tmp << (8 - bit_offset%8);
	}
}

void
bitclear(uint8_t *dst, int bit_offset, int nbits)
{
	dst += bit_offset/8;
	bit_offset %= 8;

	/* Special case where start and end might be in the same byte */
	if (bit_offset + nbits < 8) {
		*dst &= ~((1 << (8 - bit_offset)) - 1)
		        | ((1 << (8 - bit_offset - nbits)) - 1);
		return;
	}

	if (bit_offset) {
		*dst &= ~((1 << (8 - bit_offset)) - 1);
		dst++;
		nbits -= (8 - bit_offset);
	}

	memset(dst, 0, nbits/8);
	dst += nbits/8;

	if (nbits % 8) {
		*dst &= (1 << (8 - nbits % 8)) - 1;
	}
}

int
count_ones(const uint8_t *data, size_t len)
{
	uint8_t tmp;
	int count = 0;

	for (; len>0; len--) {
		tmp = *data++;
		for (; tmp; count++) {
			tmp &= tmp-1;
		}
	}

	return count;
}

float
ieee754_be(const uint8_t *raw)
{
	union {
		uint32_t raw;
		float value;
	} data;

	data.raw = raw[0] << 24 | raw[1] << 16 | raw[2] << 8 | raw[3];
	return data.value;
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

time_t
my_timegm(struct tm *tm)
{
    time_t ret;
    char *tz;

    tz = getenv("TZ");
    if (tz) tz = strdup(tz);
    setenv("TZ", "", 1);
    tzset();
    ret = mktime(tm);
    if (tz) {
        setenv("TZ", tz, 1);
        free(tz);
    } else {
        setenv("TZ", "", 1);
	}
    tzset();
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

float
wv_sat_pressure(float temp)
{
	const float coeffs[] = {-0.493158, 1 + 4.6094296e-3, -1.3746454e-5, 1.2743214e-8};
	float T, p;
	int i;

	temp += 273.15f;

	T = 0;
	for (i=LEN(coeffs)-1; i>=0; i--) {
		T *= temp;
		T += coeffs[i];
	}

	p = expf(-5800.2206f / T
		  + 1.3914993f
		  + 6.5459673f * logf(T)
		  - 4.8640239e-2f * T
		  + 4.1764768e-5f * T * T
		  - 1.4452093e-8f * T * T * T);

	return p / 100.0f;

}

float
cspline(const float *xs, const float *ys, float count, float x)
{
	int i;
	float m_i, m_next_i, t, y;

	for (i=1; i<count-1; i++) {
		if (x < xs[i+1]) {
			/* Compute tangents at xs[i] and xs[i+1] */
			m_i = spline_tangent(xs, ys, i) / (xs[i+1] - xs[i]);
			m_next_i = spline_tangent(xs, ys, i+1) / (xs[i+1] - xs[i]);

			/* Compute spline transform between xs[i] and xs[i+1] */
			t = (x - xs[i]) / (xs[i+1] - xs[i]);

			y = hermite_00(t) * ys[i]
			  + hermite_10(t) * (xs[i+1] - x) * m_i
			  + hermite_01(t) * ys[i+1]
			  + hermite_11(t) * (xs[i+1] - x) * m_next_i;

			return y;
		}
	}
	return -1;
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
			"                           Supported values: rs41, dfm, m10, ims100\n"
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

static float
spline_tangent(const float *xs, const float *ys, int k)
{
	return 0.5 * ((ys[k+1] - ys[k]) / (xs[k+1] - xs[k])
	            + (ys[k] - ys[k-1]) / (xs[k] - xs[k-1]));
}
static float hermite_00(float x) { return (1 + 2*x) * (1-x)*(1-x); }
static float hermite_10(float x) { return x * (1-x) * (1-x); }
static float hermite_01(float x) { return x * x * (3 - 2*x); }
static float hermite_11(float x) { return x * x * (x - 1); }
