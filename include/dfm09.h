#ifndef dfm09_h
#define dfm09_h

#include "data.h"

typedef struct dfm09decoder DFM09Decoder;

/**
 * Initialize a Graw dfm09 frame decoder
 *
 * @param samplerate samplerate of the raw FM-demodulated stream
 * @return an initialized decoder object
 */
DFM09Decoder* dfm09_decoder_init(int samplerate);

/**
 * Deinitialize the given decoder
 *
 * @param d deocder to deinit
 */
void dfm09_decoder_deinit(DFM09Decoder *d);

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
ParserStatus dfm09_decode(DFM09Decoder *d, SondeData *dst, const float *src, size_t len);



#endif
