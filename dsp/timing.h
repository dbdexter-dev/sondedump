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
 * Initialize M&M symbol timing estimator
 *
 * @param sym_freq expected symbol frequency
 * @param bw bandwidth of the loop filter
 */
void timing_init(Timing *t, float sym_freq, float alpha);

/**
 * Update symbol timing estimate
 *
 * @param sample sample to update the estimate with
 */
void retime(Timing *t, float interm, float sample);

/**
 * Advance the internal symbol clock by one sample (not symbol, sample)
 */
int advance_timeslot(Timing *t);

#endif
