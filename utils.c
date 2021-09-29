#include <math.h>
#include <stdio.h>
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

char*
gen_fname()
{
	time_t t;
	struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);

	strftime(_generated_fname, sizeof(_generated_fname), "LRPT_%Y_%m_%d-%H_%M.s", tm);

	return _generated_fname;
}

void
humanize(size_t count, char *buf)
{
	const char suffix[] = " kMGTPE";
	float fcount;
	int exp_3;

	if (count < 1000) {
		sprintf(buf, "%lu %c", count, suffix[0]);
	} else {
		for (exp_3 = 0, fcount = count; fcount > 1000; fcount /= 1000, exp_3++)
			;
		if (fcount > 99.9) {
			sprintf(buf, "%3.f %c", fcount, suffix[exp_3]);
		} else if (fcount > 9.99) {
			sprintf(buf, "%3.1f %c", fcount, suffix[exp_3]);
		} else {
			sprintf(buf, "%3.2f %c", fcount, suffix[exp_3]);
		}
	}
}

void
seconds_to_str(unsigned secs, char *buf)
{
	unsigned h, m, s;

	if (secs > 99*60*60) {
		sprintf(buf, "00:00:00");
		return;
	}

	s = secs % 60;
	m = (secs / 60) % 60;
	h = secs / 3600;
	sprintf(buf, "%02u:%02u:%02u", h, m, s);
}

float
human_to_float(const char *human)
{
	int ret;
	float tmp;
	const char *suffix;

	tmp = atof(human);

	/* Search for the suffix */
	for (suffix=human; (*suffix >= '0' && *suffix <= '9') || *suffix == '.'; suffix++);

	switch(*suffix) {
		case 'k':
		case 'K':
			ret = tmp * 1000;
			break;
		case 'M':
			ret = tmp * 1000000;
			break;
		default:
			ret = tmp;
			break;
	}

	return ret;

}

float
altitude_to_pressure(float alt)
{
	return 1013.25f * powf(1.0f - 2.25577f * 1e-5f * alt, 5.25588f);
}

float
dewpt(float temp, float rh)
{
	const float tmp = (logf(rh / 100.0f) + (17.27f * temp / (237.3f + temp))) / 17.27f;
	return 237.3f * tmp  / (1 + tmp);
}

void
usage(const char *pname)
{
	fprintf(stderr, "Usage: %s [options] file_in\n", pname);
	fprintf(stderr,
			"   -f, --fmt <format>      Format output lines as <format>"
			"   -g, --gpx <file>        Output GPX track to <file>\n"
			"   -k, --kml <file>        Output KML track to <file>\n"
			"   -l, --live-kml <file>   Output live KML track to <file>\n"

	        "\n"
	        "   -h, --help              Print this help screen\n"
	        "   -v, --version           Print version info\n"
	        );
	fprintf(stderr,
			"\nAvailable format specifiers:\n"
			"   %%a      Altitude (m)\n"
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
	fprintf(stderr, "meteor_demod v" VERSION "\n");
}
