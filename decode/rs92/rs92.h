#ifndef rs92_h
#define rs92_h

#include "decode/common.h"
#include "decode/correlator/correlator.h"
#include "decode/ecc/rs.h"
#include "demod/gfsk.h"
#include "protocol.h"

typedef struct {
	RS92Calibration data;
	uint8_t missing[sizeof(RS92Calibration)/8/RS92_CALIB_FRAGSIZE+1];
} RS92Metadata;

typedef struct {
	GFSKDemod gfsk;
	Correlator correlator;
	RSDecoder rs;
	RS92Frame frame[5];
	RS92Metadata metadata;
} RS92Decoder;

/**
 * Initialize a Vaisala RS92 frame decoder
 *
 * @param d decoder to init
 * @param samplerate samplerate of the raw FM-demodulated stream
 */
void rs92_decoder_init(RS92Decoder *d, int samplerate);

/**
 * Deinitialize the given decoder
 *
 * @param d deocder to deinit
 */
void rs92_decoder_deinit(RS92Decoder *d);

/**
 * Decode the next frame in the stream
 *
 * @param d decoder to use
 * @param read function to use to pull in new raw samples
 */
SondeData rs92_decode(RS92Decoder *d, int (*read)(float *dst));



#endif
