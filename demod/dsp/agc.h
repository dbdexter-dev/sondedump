#ifndef agc_h
#define agc_h
#include <complex.h>

typedef struct {
	float gain;
	float bias;
	float moving_avg;
} Agc;


/**
 * Initialize AGC
 */
void agc_init(Agc *agc);

/**
 * Automatic gain control loop
 *
 * @param sample sample to rescale
 * @return scaled sample
 */
float agc_apply(Agc *agc, float sample);

#endif
