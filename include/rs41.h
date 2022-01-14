#ifndef rs41_h
#define rs41_h

#include "data.h"

typedef struct rs41decoder RS41Decoder;

/**
 * Initialize a Vaisala RS41 frame decoder
 *
 * @param samplerate samplerate of the raw FM-demodulated stream
 * @return an initialized decoder object
 */
RS41Decoder* rs41_decoder_init(int samplerate);

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
SondeData rs41_decode(RS41Decoder *d, int (*read)(float *dst, size_t len));

#endif
