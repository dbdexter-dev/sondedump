#include <stdio.h>
#include <stdlib.h>
#include "rs41.h"
#include "utils.h"
#include "frame.h"
#include "subframe.h"
#include "decode/prng.h"

#ifndef NDEBUG
static FILE *debug;
#endif

void
rs41_decoder_init(RS41Decoder *d, int samplerate)
{
	correlator_init(&d->correlator, RS41_SYNCWORD, RS41_SYNC_LEN);
	gfsk_init(&d->gfsk, samplerate, RS41_BAUDRATE);
	d->state = READ;

#ifndef NDEBUG
	debug = fopen("/tmp/rs41frames.data", "wb");
#endif
}

void
rs41_decoder_deinit(RS41Decoder *d)
{
#ifndef NDEBUG
	fclose(debug);
#endif
	gfsk_deinit(&d->gfsk);
}

int
rs41_decode(RS41Decoder *d, int (*read)(float *dst))
{
	RS41Frame *frame;
	RS41Subframe *subframe;
	int frame_data_len;
	int offset;
	int inverted;
	int i;

	switch (d->state) {
		case READ:
			/* Read a frame worth of bits */
			if (!gfsk_decode(&d->gfsk, d->raw_frame, 0, RS41_MAX_FRAME_LEN*8, read)) return 0;

			/* Find the offset with the strongest correlation with the sync marker */
			offset = correlate(&d->correlator, &inverted, d->raw_frame, RS41_MAX_FRAME_LEN);

			if (offset) {
				/* Read more bits to compensate for the frame offset */
				if (!gfsk_decode(&d->gfsk, d->raw_frame + RS41_MAX_FRAME_LEN, 0, offset, read))
					return 0;
				/* Move bits to the beginning */
				bitcpy(d->raw_frame, d->raw_frame, offset, 8*RS41_MAX_FRAME_LEN);
			}

			/* Invert if necessary */
			if (inverted) {
				for (i=0; i<RS41_MAX_FRAME_LEN; i++) {
					d->raw_frame[i] ^= 0xFF;
				}
			}

			/* Descramble and error correct */
			frame = (RS41Frame*)d->raw_frame;
			rs41_frame_descramble(frame);
			if (rs41_frame_correct(frame) < 0) return 2;

#ifndef NDEBUG
			/* Output the frame to file */
			fwrite(d->raw_frame, RS41_MAX_FRAME_LEN, 1, debug);
#endif

			/* Prepare to parse subframes */
			d->offset = 0;
			d->state = PARSE_SUBFRAME;
			__attribute__((fallthrough));

		case PARSE_SUBFRAME:
			/* Parse expected data length from extended flag */
			frame = (RS41Frame*)d->raw_frame;
			frame_data_len = RS41_DATA_LEN + (rs41_frame_is_extended(frame) ? RS41_XDATA_LEN : 0);

			/* Update pointer to the subframe */
			subframe = (RS41Subframe*)&frame->data[d->offset];
			d->offset += subframe->len + 4;

			/* If the subframe is within frame bounds, process it. Otherwise,
			 * read another frame */
			if (d->offset < frame_data_len) {
				rs41_parse_subframe(subframe);
			} else {
				d->state = READ;
			}
			break;
		default:
			break;
	}

	return 1;
}
