#include <stdio.h>
#include "decode/framer.h"
#include "decode/manchester.h"
#include "frame.h"
#include "ims100.h"
#include "protocol.h"
#include "utils.h"

#ifndef NDEBUG
static FILE *debug;
#endif

enum { READ, PARSE_EVEN_TIME, PARSE_EVEN_POS, PARSE_ODD, PARSE_END };

void
ims100_decoder_init(IMS100Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, IMS100_BAUDRATE);
	correlator_init(&d->correlator, IMS100_SYNCWORD, IMS100_SYNC_LEN);
	bch_init(&d->rs, IMS100_REEDSOLOMON_N, IMS100_REEDSOLOMON_K,
			IMS100_REEDSOLOMON_POLY, ims100_bch_roots);

	d->state = READ;
#ifndef NDEBUG
	debug = fopen("/tmp/ims100frames.data", "wb");
#endif
}

void
ims100_decoder_deinit(IMS100Decoder *d)
{
	gfsk_deinit(&d->gfsk);
#ifndef NDEBUG
	fclose(debug);
#endif
}

SondeData
ims100_decode(IMS100Decoder *self, int (*read)(float *dst))
{
	SondeData data = {.type = EMPTY};
	uint8_t *const raw_frame = (uint8_t*)self->frame;
	unsigned int seq;

	switch (self->state) {
		case READ:
			/* Read a new frame */
			if (read_frame_gfsk(&self->gfsk, &self->correlator, raw_frame, read, IMS100_FRAME_LEN, 0) < 0) {
				data.type = SOURCE_END;
				return data;
			}

			manchester_decode(raw_frame, raw_frame, IMS100_FRAME_LEN);
			ims100_frame_descramble(self->frame);
			ims100_frame_error_correct(self->frame, &self->rs);

			seq = IMS100Frame_seq(self->frame);
			self->state = (seq & 0x01) ? PARSE_ODD : PARSE_EVEN_TIME;
			if (seq & 0x1) {
			} else {
				ims100_frame_unpack_even(&self->even, self->frame);
			}

			break;

		/* Even frame parsing {{{ */
		case PARSE_EVEN_TIME:
#ifndef NDEBUG
			fwrite(&self->even, sizeof(self->even), 1, debug);
			fflush(debug);
#endif

			data.type = DATETIME;
			data.data.datetime.datetime = IMS100FrameEven_time(&self->even);

			self->state = PARSE_EVEN_POS;
			break;
		case PARSE_EVEN_POS:
			self->state = PARSE_END;
			break;
		/* }}} */

		/* Odd frame parsing {{{ */
		case PARSE_ODD:
			self->state = PARSE_END;
			break;
		/* }}} */

		case PARSE_END:
			data.type = FRAME_END;
			self->state = READ;
			break;
		default:
			break;
	}

	return data;
}

