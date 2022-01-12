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


/**
 * Copy bits from one place to the other. Starting bit may or may not be
 * byte-aligned.
 *
 * @param dst buffer to write bits to
 * @param src buffer to read bits from
 * @param src_offset offset of the first bit to copy
 * @param num_bits number of bits to copy
 */
void bitcpy(uint8_t *dst, const uint8_t *src, size_t src_offset, size_t num_bits);

/**
 * Merge contiguous bits into a number
 *
 * @param data pointer to raw data bytes
 * @param nbits number of bits to merge
 * @return merged bits
 */
uint64_t bitmerge(const uint8_t *data, int nbits);

/**
 * Pack loose bits together, with an optional offset
 *
 * @param dst buffer to write bits to
 * @param src buffer to read bits from
 * @param bit_offset dst offset to start writing bits from
 * @param nbits number of bits to write
 */
void bitpack(uint8_t *dst, const uint8_t *src, int bit_offset, int nbits);

/**
 * Clear bits within a buffer
 *
 * @param dst buffer to clear bits from
 * @param bit_offset offset from the start of dst to write zeros from, in bits
 * @param nbits number of bits to clear
 */
void bitclear(uint8_t *dst, int bit_offset, int nbits);

/**
 * Count the number of bits set within a byte array
 *
 * @param data input array
 * @param len length of the input arary, in bytes
 * @return number of bits set in the array
 */
int count_ones(const uint8_t *data, size_t len);

/**
 * Convert 4 bytes into a IEEE754 float
 */
float ieee754_be(const uint8_t *raw);

/**
 * strdup, but portable since it's not part of the C standard and has a
 * different name based on the platform
 *
 * @param str string to duplicate
 * @return duplicated string, allocated on the heap with malloc(0
 */
char *my_strdup(char *str);

/**
 * timegm, but portable since it's not part of the C standard
 *
 * @param tm UTC time decomposed as if by gmtime
 * @return UTC time, such that gmtime(my_timegm(tm)) = tm
 */
time_t my_timegm(struct tm *tm);

/**
 * Calculate pressure at a given altitude
 *
 * @param alt altitude
 * @return pressure at that altitude (hPa)
 */
float altitude_to_pressure(float alt);

/**
 * Calculate dew point
 *
 * @param temp temperature (degrees Celsius)
 * @param rh relative humidity (0-100%)
 * @return dew point (degrees Celsius)
 */
float dewpt(float temp, float rh);

/**
 * Calculate saturation mixing ratio given temperature and pressure
 *
 * @param temp temperature (degrees Celsius)
 * @param p pressure (hPa)
 * @return saturation mixing ratio (g/kg)
 */
float sat_mixing_ratio(float temp, float p);

/**
 * Calculate the water vapour saturation pressure at a temeprature (Hyland/Wexler)
 *
 * @param temp temperature ('C)
 * @return saturation pressure (Pa)
 */
float wv_sat_pressure(float temp);

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
void  usage(const char *progname);

/**
 * Write version info to stdout
 */
void  version();

#endif


