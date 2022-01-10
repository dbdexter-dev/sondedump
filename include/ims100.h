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
 * @param read function to use to pull in new raw samples
 */
SondeData ims100_decode(IMS100Decoder *d, int (*read)(float *dst));


#endif
