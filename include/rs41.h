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
 * @param dst pointer to data struct to fill
 * @param src pointer to raw samples to decode
 * @param len number of samples available
 *
 * @return PROCEED if the src buffer has been fully processed
 *         PARSED  if a frame has been decoded into *dst
 */
ParserStatus rs41_decode(RS41Decoder *self, SondeData *dst, const float *src, size_t len);

#endif
