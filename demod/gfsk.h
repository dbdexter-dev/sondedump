#ifndef gfsk_h
#define gfsk_h

#include <stdint.h>
#include <stdlib.h>
#include "include/data.h"
#include "dsp/agc.h"
#include "dsp/filter.h"
#include "dsp/timing.h"

#define GFSK_FILTER_ORDER 24
#define SYM_ZETA 0.707
#define BUFLEN 1024
#define MIN_SAMPLES_PER_SYMBOL 8

typedef struct {
	int samplerate, symrate;
	Agc agc;
	Filter lpf;
	Timing timing;

	size_t src_offset;
	float interm;
} GFSKDemod;

/**
 * Initialize the GFSK decoder
 *
 * @param samplerate expected input sample rate
 * @param symrate expected output symbol rate
 */
int gfsk_init(GFSKDemod *g, int samplerate, int symrate);

/**
 * Deinitialize the GFSK decoder
 */
void gfsk_deinit(GFSKDemod *g);

/**
 * Demod bits from a GFSK-coded sample stream
 *
 * @param dst destination buffer where the bits will be written to
 * @param bit_offset offset from the start of dst where bits should be written, in bits
 * @param count number of bits to decode
 * @param src pointer to samples to demodulate
 * @param len number of samples in the input buffer
 *
 * @return offset of the last bit decoded
 */
ParserStatus gfsk_demod(GFSKDemod *g, uint8_t *dst, size_t *bit_offset, size_t count, const float *src, size_t len);

#endif
