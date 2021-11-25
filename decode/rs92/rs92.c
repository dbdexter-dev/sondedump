#include <stdio.h>
#include "decode/manchester.h"
#include "rs92.h"

#ifndef NDEBUG
static FILE *debug;
#endif


void
rs92_decoder_init(RS92Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, RS92_BAUDRATE);
#ifndef NDEBUG
	debug = fopen("/tmp/rs92frames.data", "wb");
#endif
}


void
rs92_decoder_deinit(RS92Decoder *d)
{
	gfsk_deinit(&d->gfsk);
#ifndef NDEBUG
	fclose(debug);
#endif
}


SondeData
rs92_decode(RS92Decoder *self, int (*read)(float *dst))
{
	SondeData data;

	/* Read a new frame worth of bits */
	if (!gfsk_demod(&self->gfsk, (uint8_t*)self->frame, 0, RS92_FRAME_LEN*8, read)) {
		data.type = SOURCE_END;
		return data;
	}

	manchester_decode((uint8_t*)self->frame, (uint8_t*)self->frame, 0, RS92_DECODED_FRAME_LEN);

#ifndef NDEBUG
	fwrite((uint8_t*)self->frame, RS92_DECODED_FRAME_LEN, 1, debug);
#endif

	data.type = EMPTY;
	return data;
}
