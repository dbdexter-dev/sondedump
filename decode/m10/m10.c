#include <stdio.h>
#include "decode/manchester.h"
#include "m10.h"

#ifndef NDEBUG
static FILE *debug;
#endif


void
m10_decoder_init(M10Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, M10_BAUDRATE);
	correlator_init(&d->correlator, 0xacd995965a556665, 8);

#ifndef NDEBUG
	debug = fopen("/tmp/m10frames.data", "wb");
#endif
}

void
m10_decoder_deinit(M10Decoder *d)
{
	gfsk_deinit(&d->gfsk);

#ifndef NDEBUG
	fclose(debug);
#endif
}

SondeData
m10_decode(M10Decoder *self, int (*read)(float *dst))
{
	SondeData data;
	int i;
	int offset, inverted;

	inverted = 0;
	if (!gfsk_demod(&self->gfsk, (uint8_t*)self->frame, 0, M10_FRAME_LEN*8, read)) {
		data.type = SOURCE_END;
		return data;
	}
	offset = correlate(&self->correlator, &inverted, (uint8_t*)self->frame, M10_FRAME_LEN);

	if (offset) {
		if (!gfsk_demod(&self->gfsk, (uint8_t*)self->frame + M10_FRAME_LEN, 0, offset, read)) {
			data.type = SOURCE_END;
			return data;
		}

		bitcpy((uint8_t*)self->frame, (uint8_t*)self->frame, offset, M10_FRAME_LEN*8);
	}

	if (inverted) {
		for (i=0; i<sizeof(*self->frame); i++) {
			((uint8_t*)self->frame)[i] ^= 0xFF;
		}
	}

	//manchester_decode((uint8_t*)self->frame, (uint8_t*)self->frame, 0, M10_FRAME_LEN*4);

#ifndef NDEBUG
	fwrite(self->frame, sizeof(*self->frame), 1, debug);
#endif


	data.type = EMPTY;
	return data;
}
