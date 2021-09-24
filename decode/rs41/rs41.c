#include <stdio.h>
#include <stdlib.h>
#include "rs41.h"
#include "utils.h"
#include "frame.h"
#include "subframe.h"
#include "string.h"
#include "ecc/crc.h"
#include "decode/prng.h"

static void rs41_update_metadata(RS41Metadata *m, RS41Subframe_Status *s);

#ifndef NDEBUG
static FILE *debug;
#endif


void
rs41_decoder_init(RS41Decoder *d, int samplerate)
{
	correlator_init(&d->correlator, RS41_SYNCWORD, RS41_SYNC_LEN);
	gfsk_init(&d->gfsk, samplerate, RS41_BAUDRATE);
	rs_init(&d->rs, RS41_REEDSOLOMON_N, RS41_REEDSOLOMON_K, RS41_REEDSOLOMON_POLY,
			RS41_REEDSOLOMON_FIRST_ROOT, RS41_REEDSOLOMON_ROOT_SKIP);

	d->state = READ;
	d->metadata.data = NULL;
	d->metadata.len = 0;

#ifndef NDEBUG
	debug = fopen("/tmp/rs41frames.data", "wb");
#endif
}

void
rs41_decoder_deinit(RS41Decoder *d)
{
	gfsk_deinit(&d->gfsk);
	rs_deinit(&d->rs);
	if (d->metadata.data) {
		free(d->metadata.data);
	}

#ifndef NDEBUG
	fclose(debug);
#endif
}

int
rs41_decode(RS41Decoder *self, int (*read)(float *dst))
{
	RS41Subframe *subframe;
	uint8_t tmp;
	int frame_data_len;
	int offset;
	int inverted;
	int i, errors;

	switch (self->state) {
		case READ:
			/* Read a frame worth of bits */
			if (!gfsk_decode(&self->gfsk, (uint8_t*)self->frame, 0, RS41_MAX_FRAME_LEN*8, read)) return 0;

			/* Find the offset with the strongest correlation with the sync marker */
			offset = correlate(&self->correlator, &inverted, (uint8_t*)self->frame, RS41_MAX_FRAME_LEN);
			if (offset) {
				/* Read more bits to compensate for the frame offset */
				if (!gfsk_decode(&self->gfsk, (uint8_t*)self->frame + RS41_MAX_FRAME_LEN, 0, offset, read))
					return 0;
				bitcpy((uint8_t*)self->frame, (uint8_t*)self->frame, offset, 8*RS41_MAX_FRAME_LEN);
			}

			/* Invert if necessary */
			if (inverted) {
				for (i=0; i<RS41_MAX_FRAME_LEN; i++) {
					((uint8_t*)self->frame)[i] ^= 0xFF;
				}
			}

			/* Descramble and error correct */
			rs41_frame_descramble(self->frame);
			errors = rs41_frame_correct(self->frame, &self->rs);
			printf("%d\n", errors);
			if (errors < 0) return 2;

#ifndef NDEBUG
			/* Output the frame to file */
			fwrite(self->frame, RS41_MAX_FRAME_LEN, 1, debug);
#endif

			/* Prepare to parse subframes */
			self->offset = 0;
			self->state = PARSE_SUBFRAME;
			__attribute__((fallthrough));

		case PARSE_SUBFRAME:
			/* Parse expected data length from extended flag */
			frame_data_len = RS41_DATA_LEN + (rs41_frame_is_extended(self->frame) ? RS41_XDATA_LEN : 0);

			/* Update pointer to the subframe */
			subframe = (RS41Subframe*)&self->frame->data[self->offset];
			self->offset += subframe->len + 4;

			/* If the frame ends out of bounds, we reached the end: read next */
			if (self->offset >= frame_data_len) {
				self->state = READ;
				break;
			}

			/* Validate the subframe's checksum */
			tmp = subframe->data[subframe->len];
			subframe->data[subframe->len] = subframe->data[subframe->len+1];
			subframe->data[subframe->len+1] = tmp;
			if (crc16_ccitt_false(subframe->data, subframe->len+2)) break;

			/* Parse the subframe */
			switch (subframe->type) {
				case RS41_SFTYPE_EMPTY:
					/* Padding */
					break;
				case RS41_SFTYPE_INFO:
					/* Frame sequence number, serial no., board info, calibration data */
					rs41_update_metadata(&self->metadata, (RS41Subframe_Status*)subframe);
					break;
				case RS41_SFTYPE_PTU:
					/* Temperature, humidity, pressure */
					break;
				default:
					/* Unknown */
					break;
			}


			break;
		default:
			break;
	}

	return 1;
}

static void
rs41_update_metadata(RS41Metadata *m, RS41Subframe_Status *s)
{
	size_t frag_offset;

	/* Allocate enough space to contain the misc metadata */
	if (!m->len) {
		m->len = (s->frag_count+1) * 16;
		m->data = malloc(m->len);
		m->missing = (1 << (s->frag_count+1)) - 1;
	}

	/* Copy the fragment and update the bitmap of the fragments left */
	frag_offset = s->frag_seq * LEN(s->frag_data);
	memcpy(m->data + frag_offset, s->frag_data, LEN(s->frag_data));
	m->missing &= ~(1 << s->frag_seq);

}
