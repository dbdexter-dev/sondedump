#ifndef ims100_h
#define ims100_h

#include "data.h"

typedef struct ims100decoder IMS100Decoder;

/**
 * Initialize a Meisei ims100 frame decoder
 *
 * @param samplerate samplerate of the raw FM-demodulated stream
 * @return an initialized decoder object
 */
IMS100Decoder *ims100_decoder_init(int samplerate);

/**
 * Deinitialize the given decoder
 *
 * @param d deocder to deinit
 */
void ims100_decoder_deinit(IMS100Decoder *d);

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
ParserStatus ims100_decode(IMS100Decoder *d, SondeData *dst, const float *src, size_t len);


#endif
