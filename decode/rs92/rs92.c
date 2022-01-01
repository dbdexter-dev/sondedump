#include <stdio.h>
#include <string.h>
#include "decode/correlator/correlator.h"
#include "decode/ecc/crc.h"
#include "decode/manchester.h"
#include "frame.h"
#include "rs92.h"

#ifndef NDEBUG
static FILE *debug;
#endif

enum { READ, PARSE_SUBFRAME };

void
rs92_decoder_init(RS92Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, RS92_BAUDRATE);
	correlator_init(&d->correlator, RS92_SYNCWORD, RS92_SYNC_LEN);

	rs_init(&d->rs, RS92_REEDSOLOMON_N, RS92_REEDSOLOMON_K, RS92_REEDSOLOMON_POLY,
			RS92_REEDSOLOMON_FIRST_ROOT, RS92_REEDSOLOMON_ROOT_SKIP);

	memset(&d->metadata.missing, 0xFF, sizeof(d->metadata.missing));
	d->metadata.missing[sizeof(d->metadata.missing)-1] &= ~((1 << (7 - RS92_CALIB_FRAGCOUNT%8)) - 1);
	d->state = READ;
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
	SondeData data = {.type = EMPTY};
	int offset, inverted;
	int i;
	uint8_t *raw_frame = (uint8_t*)self->frame;
	RS92Subframe* subframe;
	RS92Subframe_Info *info;


	switch (self->state) {
		case READ:
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
			rs92_frame_correct(self->frame, &self->rs);

#ifndef NDEBUG
			fwrite(raw_frame, RS92_BARE_FRAME_LEN, 1, debug);
#endif

			/* Prepare to parse subframes */
			self->offset = 0;
			self->state = PARSE_SUBFRAME;
			__attribute__((fallthrough));

		case PARSE_SUBFRAME:
			/* Update pointer to subframe */
			subframe = (RS92Subframe*)&self->frame->data[self->offset];
			self->offset += subframe->len * 2 + 4;

			/* If the frame ends out of bounds, we reached the end: read next */
			if (self->offset >= RS92_DATA_LEN) {
				data.type = FRAME_END;
				self->state = READ;
				break;
			}

			/* Validate the subframe's checksum against the one received.
			 * If it doesn't match, discard it */
			if (crc16_ccitt_false(subframe->data, 2*subframe->len) != *(uint16_t*)&subframe->data[2*subframe->len]) {
				data.type = EMPTY;
				break;
			}

			switch (subframe->type) {
				case RS92_SFTYPE_INFO:
					info = (RS92Subframe_Info*)subframe;

					data.type = INFO;
					data.data.info.seq = info->seq;
					data.data.info.sonde_serial = info->serial + 2; /* Skip whitespace */
					data.data.info.burstkill_status = -1;

					break;
				case RS92_SFTYPE_PTU:
					break;
				case RS92_SFTYPE_GPSINFO:
					break;
				case RS92_SFTYPE_GPSRAW:
					break;
				default:
					break;
			}
	}

	return data;
}
