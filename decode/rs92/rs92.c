#include <stdio.h>
#include "decode/manchester.h"
#include "decode/correlator/correlator.h"
#include "frame.h"
#include "rs92.h"

#ifndef NDEBUG
static FILE *debug;
#endif


void
rs92_decoder_init(RS92Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, RS92_BAUDRATE);
	correlator_init(&d->correlator, RS92_SYNCWORD, RS92_SYNC_LEN);
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
	int offset, inverted;
	int i;
	uint8_t *raw_frame = (uint8_t*)self->frame;

	/* Read a new frame worth of bits */
	if (!gfsk_demod(&self->gfsk, raw_frame, 0, RS92_FRAME_LEN, read)) {
		data.type = SOURCE_END;
		return data;
	}

	/* Find synchronization marker */
	offset = correlate(&self->correlator, &inverted, raw_frame, RS92_FRAME_LEN);

	/* Compensate offset */
	if (offset) {
		if (!gfsk_demod(&self->gfsk, raw_frame + RS92_FRAME_LEN/8, 0, offset, read)) {
			data.type = SOURCE_END;
			return data;
		}

		bitcpy(raw_frame, raw_frame, offset, RS92_FRAME_LEN);
	}

	/* Correct phase errors */
	if (inverted) {
		for (i=0; i<RS92_FRAME_LEN*2; i++) {
			raw_frame[i] ^= 0xFF;
		}
	}

	/* Manchester decode, then remove padding and reorder bits */
	manchester_decode((uint8_t*)self->frame, (uint8_t*)self->frame, RS92_FRAME_LEN);
	rs92_frame_descramble(self->frame);

#ifndef NDEBUG
	fwrite((uint8_t*)self->frame, RS92_BARE_FRAME_LEN, 1, debug);
#endif

	data.type = EMPTY;
	return data;
}
