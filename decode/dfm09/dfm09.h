#ifndef dfm09_h
#define dfm09_h

#include "decode/common.h"
#include "decode/correlator/correlator.h"
#include "demod/gfsk.h"
#include "protocol.h"

typedef struct {
	GFSKDemod gfsk;
	Correlator correlator;
	DFM09Frame frame[4];
	DFM09ParsedFrame parsed_frame;
	DFM09Calib calib;
	struct tm gps_time;
	int gps_idx, ptu_type_serial;
	SondeData gps_data, ptu_data;
	int state;

	char serial[10];
	uint64_t raw_serial;
} DFM09Decoder;

/**
 * Initialize a Vaisala dfm09 frame decoder
 *
 * @param d decoder to init
 * @param samplerate samplerate of the raw FM-demodulated stream
 */
void dfm09_decoder_init(DFM09Decoder *d, int samplerate);

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
SondeData dfm09_decode(DFM09Decoder *d, int (*read)(float *dst));



#endif
