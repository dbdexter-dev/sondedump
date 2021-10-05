#ifndef timing_h
#define timing_h

#include <complex.h>

typedef struct {
	float prev;
	float phase, freq;
	float alpha, beta;
	int state;
} Timing;


/**
 * Initialize Gardner symbol timing estimator
 *
 * @param t symbol timing estimator
 * @param sym_freq expected symbol frequency
 * @param bw bandwidth of the loop filter
 */
void timing_init(Timing *t, float sym_freq, float alpha);

/**
 * Update symbol timing estimate
 *
 * @param t symbol timing estimator
 * @param sample sample to update the estimate with
 */
void retime(Timing *t, float interm, float sample);

/**
 * Advance the internal symbol clock by one sample (not symbol, sample)
 *
 * @param t symbol timing estimator
 * @return 1 on intersample
 *         2 on sample
 *         0 otherwise
 */
int advance_timeslot(Timing *t);

#endif
