#ifndef rs41_h
#define rs41_h

#include "correlator/correlator.h"
#include "demod/gfsk.h"
#include "ecc/rs.h"
#include "protocol.h"

typedef struct {
	uint8_t *data;
	size_t len;
	uint64_t missing;
} RS41Metadata;

typedef struct {
	GFSKDemod gfsk;
	Correlator correlator;
	RSDecoder rs;

	RS41Frame frame[2];
	enum { READ, PARSE_SUBFRAME } state;
	int offset;
	RS41Metadata metadata;
} RS41Decoder;

/**
 * Initialize a Vaisala RS41 frame decoder
 *
 * @param d decoder to init
 * @param samplerate samplerate of the raw FM-demodulated stream
 */
void rs41_decoder_init(RS41Decoder *d, int samplerate);

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
 * @param read function to use to pull in new raw samples
 */
int rs41_decode(RS41Decoder *d, int (*read)(float *dst));

#endif
