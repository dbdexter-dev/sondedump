#include <stdio.h>
#include <string.h>
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

	rs_init(&d->rs, RS92_REEDSOLOMON_N, RS92_REEDSOLOMON_K, RS92_REEDSOLOMON_POLY,
			RS92_REEDSOLOMON_FIRST_ROOT, RS92_REEDSOLOMON_ROOT_SKIP);

	memset(&d->metadata.missing, 0xFF, sizeof(d->metadata.missing));
	d->metadata.missing[sizeof(d->metadata.missing)-1] &= ~((1 << (7 - RS92_CALIB_FRAGCOUNT%8)) - 1);
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
	offset = correlate(&self->correlator, &inverted, raw_frame, RS92_FRAME_LEN/8);

	/* Compensate offset */
	if (offset) {
		if (!gfsk_demod(&self->gfsk, raw_frame + RS92_FRAME_LEN/8, 0, offset, read)) {
			data.type = SOURCE_END;
			return data;
		}

		bitcpy(raw_frame, raw_frame, offset, RS92_FRAME_LEN);
	}

	/* Correct potentially inverted phase */
	if (inverted) {
		for (i=0; i<2*RS92_FRAME_LEN/8; i++) {
			raw_frame[i] ^= 0xFF;
		}
	}

	/* Manchester decode, then remove padding and reorder bits */
	manchester_decode(raw_frame, raw_frame, RS92_FRAME_LEN);
	rs92_frame_descramble(self->frame);

	/* Error correct */
	printf("%d\n", rs92_frame_correct(self->frame, &self->rs));

#ifndef NDEBUG
	fwrite(raw_frame, RS92_BARE_FRAME_LEN, 1, debug);
#endif

	data.type = EMPTY;
	return data;
}
