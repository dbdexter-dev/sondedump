#ifdef _MSC_VER
#include <windows.h>
#include <fileapi.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "utils.h"
#include "log/log.h"

static float spline_tangent(const float *xs, const float *ys, int k);
static float hermite_00(float x);
static float hermite_01(float x);
static float hermite_10(float x);
static float hermite_11(float x);

static unsigned int count_days(unsigned int year, unsigned int month, unsigned int day);

char
*my_strdup(const char *str)
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
my_timegm(const struct tm *tm)
{
	time_t time;

	time = tm->tm_sec + tm->tm_min * 60 + tm->tm_hour * 3600;
	time += 86400UL * (count_days(1900 + tm->tm_year, tm->tm_mon, tm->tm_mday)
	                  - count_days(1970, 0, 1));

	return time;
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

size_t
fread_all(uint8_t **buf, FILE *fd)
{
	const size_t chunksize = 1024;
	size_t len = 0;

	*buf = 0;
	while (!feof(fd)) {
		*buf = realloc(*buf, len + chunksize);
		len += fread(*buf + len, 1, chunksize, fd);
	}

	return len;
}

float
lat_to_y(float lat, int zoom)
{
	const float lat_rad = lat * M_PI/180.0;

	return (1 << zoom) * (1.0 - logf(tanf(lat_rad) + 1.0/cosf(lat_rad)) / M_PI) / 2.0;
}

float
lon_to_x(float lon, int zoom)
{
	lon = fmod(lon, 360.0);
	return (1 << zoom) * (lon  + 180.0) / 360.0;
}

float
y_to_lat(float y, int zoom)
{
	float n = M_PI - 2.0 * M_PI * y / (1 << zoom);
	return 180.0 / M_PI * atanf(0.5 * (expf(n) - expf(-n)));
}

float
x_to_lon(float x, int zoom)
{
	return x / (1 << zoom) * 360.0 - 180.0;
}


#ifdef _MSC_VER
void
rm_rf(const char *path)
{
	char filepath[512];
	HANDLE *dr;
	WIN32_FIND_DATA find_data;

	sprintf(filepath, "%s*", path);
	dr = FindFirstFileA(filepath, &find_data);
	if (!dr) {
		log_error("Failed to open directory %s", path);
		return;
	}

	do {
		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (strcmp(find_data.cFileName, ".") && strcmp(find_data.cFileName, "..")) {
				sprintf(filepath, "%s%s\\", path, find_data.cFileName);
				rm_rf(filepath);
			}
		} else {
			sprintf(filepath, "%s%s", path, find_data.cFileName);
			log_debug("Deleting %s", filepath);
			remove(filepath);
		}
	} while (FindNextFileA(dr, &find_data));

	FindClose(dr);
}
#else
void
rm_rf(const char *path)
{
	char filepath[512];
	struct dirent *d;
	struct stat stat;
	DIR *dr;

	/* Open directory */
	dr = opendir(path);
	if (!dr) {
		log_error("Failed to open directory %s", path);
		return;
	}

	/* Iterate through files in the directory */
	for(d = readdir(dr); d != NULL; d = readdir(dr)) {
		sprintf(filepath, "%s%s", path, d->d_name);
		log_debug("Deleting %s", filepath);

		/* If directory, recurse. If file, delete */
		lstat(filepath, &stat);
		if (stat.st_mode & S_IFDIR && strcmp(d->d_name, ".") && strcmp(d->d_name, "..")) {
			rm_rf(filepath);
		} else {
			remove(filepath);
		}
	}
}
#endif

/* Static functions {{{ */
static unsigned int
count_days(unsigned int year, unsigned int month, unsigned int day)
{
	const unsigned int days[2][12] = {
		{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
		{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335}
	};
	int is_leap;
	unsigned int day_count;

	is_leap = (!(year%4) && year%100) || !(year%400);
	day_count = days[is_leap][month] + day;

	/* Normalize day/month */
	while (day_count >= 365U + is_leap) {
		year++;
		day_count -= 365U + is_leap;
		is_leap = (!(year%4) && year%100) || !(year%400);
	}

	day_count += 365 * year
	          + (year / 4) - (year / 100) + (year / 400);


	return day_count - 1;

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
/* }}} */
