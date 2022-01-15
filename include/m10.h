#ifndef m10_h
#define m10_h

#include "data.h"

typedef struct m10decoder M10Decoder;


/**
 * Initialize a Meteomodem M10 frame decoder
 *
 * @param samplerate samplerate of the raw FM-demodulated stream
 * @return an initialized decoder object
 */
M10Decoder* m10_decoder_init(int samplerate);

/**
 * Deinitialize the given decoder
 *
 * @param d deocder to deinit
 */
void m10_decoder_deinit(M10Decoder *d);

/**
 * Decode the next frame in the stream
 *
 * @param d decoder to use
 * @param read function to use to pull in new raw samples
 */
ParserStatus m10_decode(M10Decoder *d, SondeData *dst, const float *src, size_t len);

#endif
