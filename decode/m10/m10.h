#ifndef m10_h
#define m10_h

#include "decode/common.h"
#include "decode/correlator/correlator.h"
#include "demod/gfsk.h"
#include "protocol.h"

typedef struct {
	GFSKDemod gfsk;
	Correlator correlator;
	M10Frame frame[4];
	int offset;
	enum { READ, PARSE_GPS_POS, PARSE_GPS_TIME, PARSE_PTU } state;
} M10Decoder;


void m10_decoder_init(M10Decoder *d, int samplerate);
void m10_decoder_deinit(M10Decoder *d);

SondeData m10_decode(M10Decoder *d, int (*read)(float *dst));

#endif
