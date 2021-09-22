#ifndef rs41_h
#define rs41_h

#include "correlator/correlator.h"
#include "demod/gfsk.h"
#include "protocol.h"

typedef struct {
	GFSKDemod gfsk;
	Correlator correlator;
	uint8_t raw_frame[2 * RS41_FRAME_LEN];
} RS41Decoder;

void rs41_decoder_init(RS41Decoder *d, int samplerate);
void rs41_decoder_deinit(RS41Decoder *d);
int rs41_decode(RS41Decoder *d, int (*read)(float *dst));

#endif
