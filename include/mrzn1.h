#ifndef mrzn1_h
#define mrzn1_h

#include "data.h"

typedef struct mrzn1decoder MRZN1Decoder;

/**
 * Initialize a MRZ-N1 frame decoder
 *
 * @param samplerate samplerate of the raw FM-demodulated stream
 * @return an initialized decoder object
 */
MRZN1Decoder *mrzn1_decoder_init(int samplerate);

/**
 * Deinitialize the given decoder
 *
 * @param d deocder to deinit
 */
void mrzn1_decoder_deinit(MRZN1Decoder *d);

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
ParserStatus mrzn1_decode(MRZN1Decoder *d, SondeData *dst, const float *src, size_t len);


#endif
