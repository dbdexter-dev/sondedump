#ifndef gfsk_h
#define gfsk_h

#include <stdint.h>
#include <stdlib.h>
#include "dsp/agc.h"
#include "dsp/filter.h"
#include "dsp/timing.h"

#define GFSK_FILTER_ORDER 24
#define SYM_ALPHA 5e-10

typedef struct {
	int samplerate, symrate;
	Agc agc;
	Filter lpf;
	Timing timing;
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
 * @param len number of bits to decode
 * @param read pointer to function used to read samples
 *
 * @return offset of the last bit decoded
 */
int gfsk_demod(GFSKDemod *g, uint8_t *dst, int bit_offset, size_t len, int (*read)(float *dst));

#endif
