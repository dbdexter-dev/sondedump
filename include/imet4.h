#ifndef imet4_h
#define imet4_h

#include "data.h"

typedef struct imet4decoder IMET4Decoder;

/**
 * Initialize a iMet 4 frame decoder
 *
 * @param samplerate samplerate of the raw FM-demodulated stream
 * @return an initialized decoder object
 */
IMET4Decoder *imet4_decoder_init(int samplerate);

/**
 * Deinitialize the given decoder
 *
 * @param d deocder to deinit
 */
void imet4_decoder_deinit(IMET4Decoder *d);

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
ParserStatus imet4_decode(IMET4Decoder *d, SondeData *dst, const float *src, size_t len);


#endif
