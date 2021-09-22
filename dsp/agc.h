#ifndef agc_h
#define agc_h
#include <complex.h>

/**
 * Automatic gain control loop
 *
 * @param sample sample to rescale
 * @return scaled sample
 */
float agc_apply(float sample);

/**
 * Get the current gain of the AGC
 *
 * @return gain
 */
float agc_get_gain();

#endif
