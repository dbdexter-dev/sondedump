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
	correlator_init(&d->correlator, 0x999999ac, 4);

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
	static int offset = 0;
	int inverted;
	int i;
	uint8_t topbit, tmp;

	/* Copy extra data from the previous decode */
	self->frame[0] = self->frame[1];

	/* Demod until a frame worth of bits is ready */
	if (!gfsk_demod(&self->gfsk, (uint8_t*)self->frame, offset, M10_FRAME_LEN*8-offset, read)) {
		data.type = SOURCE_END;
		return data;
	}

	/* Find sync marker */
	offset = correlate(&self->correlator, &inverted, (uint8_t*)self->frame, M10_FRAME_LEN);

	/* Align frame being decoded to sync marker */
	if (offset) {
		if (!gfsk_demod(&self->gfsk, (uint8_t*)self->frame + M10_FRAME_LEN, 0, offset, read)) {
			data.type = SOURCE_END;
			return data;
		}
		bitcpy((uint8_t*)self->frame, (uint8_t*)self->frame, offset, M10_FRAME_LEN*8);
	}

	/* Correct inverted symbol phase */
	if (inverted) {
		for (i=0; i<sizeof(*self->frame); i++) {
			((uint8_t*)self->frame)[i] ^= 0xFF;
		}
	}

	/* Manchester decode */
	manchester_decode((uint8_t*)self->frame, (uint8_t*)self->frame, 0, M10_FRAME_LEN*4);

	/* Diff decode */
	topbit = 0;
	for (i=0; i<M10_FRAME_LEN/2; i++) {
		tmp = ((uint8_t*)self->frame)[i] << 7;
		((uint8_t*)self->frame)[i] ^= topbit | ((uint8_t*)self->frame)[i] >> 1;
		topbit = tmp;
	}

#ifndef NDEBUG
	if (((uint8_t*)self->frame)[0] == 0x7f &&
		((uint8_t*)self->frame)[1] == 0xf8)
	fwrite(self->frame, sizeof(*self->frame), 1, debug);
#endif


	data.type = EMPTY;
	return data;
}
