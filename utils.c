#include <math.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"

static float spline_tangent(const float *xs, const float *ys, int k);
static float hermite_00(float x);
static float hermite_01(float x);
static float hermite_10(float x);
static float hermite_11(float x);

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
			"   -c, --csv <file>             Output data to <file> in CSV format\n"
			"   -f, --fmt <format>           Format output lines as <format>\n"
			"   -g, --gpx <file>             Output GPX track to <file>\n"
			"   -k, --kml <file>             Output KML track to <file>\n"
			"   -l, --live-kml <file>        Output live KML track to <file>\n"
			"   -r, --location <lat,lon,alt> Set receiver location to <lat, lon, alt> (default: none)\n"
			"   -t, --type <type>            Enable decoder for the given sonde type\n"
			"                                Supported values: rs41, dfm, m10, ims100\n"
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
	        "Arrow keys: change active decoder\n"
	        "Tab: toggle between absolute (lat, lon, alt) and relative (az, el, range) coordinates\n"
	       );
#endif
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

/* Static functions {{{ */
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
/* }}} */
