#include <stdio.h>
#include "decode/manchester.h"
#include "decode/correlator/correlator.h"
#include "frame.h"
#include "dfm09.h"

#ifndef NDEBUG
static FILE *debug;
#endif


void
dfm09_decoder_init(DFM09Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, DFM09_BAUDRATE);
	correlator_init(&d->correlator, DFM09_SYNCWORD, DFM09_SYNC_LEN);
#ifndef NDEBUG
	debug = fopen("/tmp/dfm09frames.data", "wb");
#endif
}


void
dfm09_decoder_deinit(DFM09Decoder *d)
{
	gfsk_deinit(&d->gfsk);
#ifndef NDEBUG
	fclose(debug);
#endif
}


SondeData
dfm09_decode(DFM09Decoder *self, int (*read)(float *dst))
{
	SondeData data;

	/* Read a new frame worth of bits */
	if (!gfsk_demod(&self->gfsk, (uint8_t*)self->frame, 0, DFM09_FRAME_LEN*2*8, read)) {
		data.type = SOURCE_END;
		return data;
	}

#ifndef NDEBUG
	fwrite((uint8_t*)self->frame, DFM09_FRAME_LEN, 1, debug);
#endif

	data.type = EMPTY;
	return data;
}
