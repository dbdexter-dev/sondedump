#ifndef utils_h
#define utils_h

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#ifndef VERSION
#define VERSION "(unknown version)"
#endif

/* setenv() and unsetenv() for Windows */
#ifdef _WIN32
#define setenv(env, val, overwrite)  _putenv_s(env, val)
#define unsetenv(env)                _putenv_s(env, "=")
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

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define LEN(x) (sizeof(x)/sizeof(x[0]))
#ifndef sgn
#define sgn(x) ((x) < 0 ? -1 : 1)
#endif
#ifndef M_PI
#define M_PI 3.1415926536
#endif

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
time_t my_timegm(struct tm *tm);

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
 * Write usage info to stdout
 *
 * @param progname executable name
 */
void usage(const char *progname);

/**
 * Write version info to stdout
 */
void version();

#endif


