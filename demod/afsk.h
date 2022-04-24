#ifndef afsk_h
#define afsk_h

#include <complex.h>
#include <stdint.h>
#include <stdlib.h>
#include "dsp/agc.h"
#include "dsp/filter.h"
#include "dsp/timing.h"
#include "include/data.h"

#define AFSK_FILTER_ORDER 24
#define AFSK_SYM_ZETA 0.707
#define MIN_SAMPLES_PER_SYMBOL 8

typedef struct {
	float f_mark, f_space;
	float p_mark, p_space;

	float complex *mark_history, *space_history;
	float complex mark_sum, space_sum;

	size_t idx, len, src_offset;

	Agc agc;
	float interm;
	Filter lpf;
	Timing timing;
} AFSKDemod;

/**
 * Initialize an AFSK decoder
 *
 * @param samplerate input sample rate
 */

int afsk_init(AFSKDemod *d, int samplerate, int symrate, float f_mark, float f_space);
void afsk_deinit(AFSKDemod *d);

ParserStatus afsk_demod(AFSKDemod *const d, uint8_t *dst, size_t *bit_offset, size_t count, const float *src, size_t len);


#endif