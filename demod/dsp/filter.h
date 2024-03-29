#ifndef filter_h
#define filter_h

#include <stdint.h>

typedef struct {
	float *mem;
	float *coeffs;

	int size;
	int num_phases;
	int idx;
} Filter;

/**
 * Initialize a FIR lowpass filter with raised cosine coefficients
 *
 * @param flt filter to initialize
 * @param order order of the filter (e.g. 16 = 16 + 1 + 16 taps)
 * @param cutoff normalized cutoff frequency, in 1/samples
 * @param num_phases number of phases in the filter
 *
 * @return 0 on success, non-zero on failure
 */

int filter_init_lpf(Filter *flt, int order, float cutoff, int num_phases);

/**
 * Feed a sample to a filter
 *
 * @param flt filter to pass the sample through
 * @param sample sample to feed
 */
void filter_fwd_sample(Filter *flt, float sample);

/**
 * Get the output of a filter
 *
 * @param flt filter to read the sample from
 * @param phase index of the phase to get the value of
 * @return filter output
 */
float filter_get(const Filter *const flt, int phase);


/**
 * Deinitialize a filter object
 *
 * @param flt filter to deinitalize
 */
void filter_deinit(Filter *flt);

#endif
