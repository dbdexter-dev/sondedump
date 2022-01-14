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
 * @param read function to use to pull in new raw samples
 */
SondeData dfm09_decode(DFM09Decoder *d, int (*read)(float *dst, size_t len));



#endif
