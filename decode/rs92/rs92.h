#ifndef rs92_h
#define rs92_h

#include "decode/common.h"
#include "demod/gfsk.h"
#include "protocol.h"

typedef struct {
	GFSKDemod gfsk;

	RS92Frame frame[2];
} RS92Decoder;

/**
 * Initialize a Vaisala RS92 frame decoder
 *
 * @param d decoder to init
 * @param samplerate samplerate of the raw FM-demodulated stream
 */
void rs92_decoder_init(RS92Decoder *d, int samplerate);

/**
 * Deinitialize the given decoder
 *
 * @param d deocder to deinit
 */
void rs92_decoder_deinit(RS92Decoder *d);

/**
 * Decode the next frame in the stream
 *
 * @param d decoder to use
 * @param read function to use to pull in new raw samples
 */
SondeData rs92_decode(RS92Decoder *d, int (*read)(float *dst));



#endif
