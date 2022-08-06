#ifndef utils_h
#define utils_h

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef VERSION
#define VERSION "(unknown version)"
#endif

/* setenv(), unsetenv() and strcasecmp() for Windows */
#ifdef _WIN32
#define setenv(env, val, overwrite)  _putenv_s(env, val)
#define unsetenv(env)                _putenv_s(env, "=")
#define strcasecmp _stricmp
#endif

/* Portable unroll pragma, for some reason clang defines __GNUC__ but uses the
 * non-GCC unroll pragma format */
#define DO_PRAGMA(x) _Pragma(#x)
#if defined(__clang__)
#define PRAGMA_UNROLL(x) DO_PRAGMA(unroll x)
#elif defined(__GNUC__)
#define PRAGMA_UNROLL(x) DO_PRAGMA(GCC unroll x)
#else
#define PRAGMA_UNROLL(x) DO_PRAGMA(unroll x)
#endif

/* Portable struct packed attribute */
#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#elif defined(_MSC_VER)
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

/* Math defines */
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define LEN(x) (sizeof(x)/sizeof(*(x)))
#ifndef sgn
#define sgn(x) ((x) < 0 ? -1 : 1)
#endif
#ifndef M_PI
#define M_PI 3.1415926536
#endif

/* Symbol size that works for relocatable executables */
#define SYMSIZE(x) (x##_size)

/**
 * strdup, but portable since it's not part of the C standard and has a
 * different name based on the platform
 *
 * @param str string to duplicate
 * @return duplicated string, allocated on the heap with malloc(0
 */
char *my_strdup(const char *str);

/**
 * timegm, but portable since it's not part of the C standard
 *
 * @param tm UTC time decomposed as if by gmtime
 * @return UTC time, such that gmtime(my_timegm(tm)) = tm
 */
time_t my_timegm(const struct tm *tm);

/**
 * Given a set of ordered coordinate pairs and a point, compute the value
 * of the cubic Hermite spline joining the given pairs at that point
 *
 * @param xs x coordinates of the known points
 * @param ys y coordinates of the known points
 * @param count number of coordinates supplied
 * @param x input to the cubic spline
 * @return output of the spline
 */
float cspline(const float *xs, const float *ys, float count, float x);

/**
 * Read a file until eof, reallocating the given buffer until it fits
 *
 * @param   buf buffer containng the data
 * @param   fd file to read data from
 * @return  grand total number of bytes read
 */
size_t fread_all(uint8_t **buf, FILE *fd);

/**
 * Recursively remove a folder
 */
void rm_rf(const char *path);

/**
 * Mercator projection latitude to y conversion
 */
float lat_to_y(float latitude, int zoom);
float y_to_lat(float y, int zoom);

/**
 * Mercator projection longitude to x conversion
 */
float lon_to_x(float longitude, int zoom);
float x_to_lon(float y, int zoom);

#endif


