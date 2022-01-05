#ifndef ims100_h
#define ims100_h

#include "decode/common.h"
#include "decode/correlator/correlator.h"
#include "decode/ecc/rs.h"
#include "demod/gfsk.h"
#include "protocol.h"

typedef struct {
	GFSKDemod gfsk;
	Correlator correlator;
	RSDecoder rs;
	IMS100Frame frame[4];
	IMS100FrameEven even;
	IMS100FrameOdd odd;
	int state;
} IMS100Decoder;

/**
 * Initialize a Meisei ims100 frame decoder
 *
 * @param d decoder to init
 * @param samplerate samplerate of the raw FM-demodulated stream
 */
void ims100_decoder_init(IMS100Decoder *d, int samplerate);

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
