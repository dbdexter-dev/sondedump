#ifndef rs41_h
#define rs41_h

#include "correlator/correlator.h"
#include "demod/gfsk.h"
#include "protocol.h"

typedef struct {
	GFSKDemod gfsk;
	Correlator correlator;
	enum { READ, PARSE_SUBFRAME } state;
	int offset;
	uint8_t raw_frame[2 * RS41_MAX_FRAME_LEN];
} RS41Decoder;

/**
 * Initialize a Vaisala RS41 frame decoder
 *
 * @param d decoder to init
 * @param samplerate samplerate of the raw FM-demodulated stream
 */
void rs41_decoder_init(RS41Decoder *d, int samplerate);

/**
 * Deinitialize the given decoder
 *
 * @param d deocder to deinit
 */
void rs41_decoder_deinit(RS41Decoder *d);

/**
 * Decode the next frame in the stream
 *
 * @param d decoder to use
 * @param read function to use to pull in new raw samples
 */
int rs41_decode(RS41Decoder *d, int (*read)(float *dst));

#endif
