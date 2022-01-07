#include <math.h>
#include <string.h>
#include "decode/framer.h"
#include "decode/manchester.h"
#include "frame.h"
#include "ims100.h"
#include "protocol.h"
#include "utils.h"

#ifndef NDEBUG
#include <stdio.h>
static FILE *debug;
#endif

enum { READ, PARSE_EVEN_INFO, PARSE_EVEN_TIME, PARSE_EVEN_POS, PARSE_EVEN_PTU, PARSE_ODD_INFO, PARSE_END };

void
ims100_decoder_init(IMS100Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, IMS100_BAUDRATE);
	correlator_init(&d->correlator, IMS100_SYNCWORD, IMS100_SYNC_LEN);
	bch_init(&d->rs, IMS100_REEDSOLOMON_N, IMS100_REEDSOLOMON_K,
			IMS100_REEDSOLOMON_POLY, ims100_bch_roots, IMS100_REEDSOLOMON_T);

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
			if (ims100_frame_error_correct(self->frame, &self->rs) < 0) return data;

			seq = IMS100Frame_seq(self->frame);
			self->state = (seq & 0x01) ? PARSE_ODD_INFO : PARSE_EVEN_INFO;
			if (seq & 0x1) {
				ims100_frame_unpack_odd(&self->odd, self->frame);
			} else {
				ims100_frame_unpack_even(&self->even, self->frame);
			}

			break;

		/* Even frame parsing {{{ */
		case PARSE_EVEN_INFO:
			/* Copy calibration data */
			if (IMS100_DATA_VALID(self->odd.valid, IMS100_MASK_SEQ | IMS100_MASK_CALIB)) {
				seq = IMS100FrameEven_seq(&self->even);
				memcpy(((uint8_t*)&self->calib) + IMS100_CALIB_FRAGSIZE * (seq % IMS100_CALIB_FRAGCOUNT),
						self->even.calib,
						sizeof(self->even.calib));
			}

#ifndef NDEBUG
			fwrite(&self->calib, sizeof(self->calib), 1, debug);
			fflush(debug);
#endif


			data.type = INFO;
			data.data.info.seq = IMS100FrameEven_seq(&self->even);
			data.data.info.sonde_serial = "IMSPlacehold";

			/* Invalidate data if subframe is marked corrupted */
			if (data.data.info.seq == -1) {
				data.type = EMPTY;
			}

			self->state = PARSE_EVEN_TIME;
			break;

		case PARSE_EVEN_TIME:
			data.type = DATETIME;
			data.data.datetime.datetime = IMS100FrameEven_time(&self->even);

			/* Invalidate data if subframe is marked corrupted */
			if (data.data.datetime.datetime == 0) {
				data.type = EMPTY;
			}

			self->state = PARSE_EVEN_POS;
			break;
		case PARSE_EVEN_POS:
			data.type = POSITION;

			data.data.pos.lat = IMS100FrameEven_lat(&self->even);
			data.data.pos.lon = IMS100FrameEven_lon(&self->even);
			data.data.pos.alt = IMS100FrameEven_alt(&self->even);
			data.data.pos.speed = IMS100FrameEven_speed(&self->even);
			data.data.pos.heading = IMS100FrameEven_heading(&self->even);

			/* Invalidate data if subframe is marked corrupted */
			if (isnan(data.data.pos.lat + data.data.pos.lon + data.data.pos.alt
			        + data.data.pos.speed + data.data.pos.heading)) {
				data.type = EMPTY;
			}

			self->state = PARSE_EVEN_PTU;
			break;
		case PARSE_EVEN_PTU:
			data.type = PTU;

			data.data.ptu.temp = IMS100FrameEven_temp(&self->even, &self->calib);
			data.data.ptu.rh = IMS100FrameEven_rh(&self->even, &self->calib);

			/* Invalidate data if subframe is marked corrupted */
			if (isnan(data.data.ptu.temp + data.data.ptu.rh)) {
				data.type = EMPTY;
			}

			self->state = PARSE_END;
			break;
		/* }}} */

		/* Odd frame parsing {{{ */
		case PARSE_ODD_INFO:
			/* Copy calibration data */
			if (IMS100_DATA_VALID(self->odd.valid, IMS100_MASK_SEQ | IMS100_MASK_CALIB)) {
				seq = IMS100FrameOdd_seq(&self->odd);
				memcpy(((uint8_t*)&self->calib) + IMS100_CALIB_FRAGSIZE * (seq % IMS100_CALIB_FRAGCOUNT),
						self->odd.calib,
						sizeof(self->odd.calib));
			}

#ifndef NDEBUG
			fwrite(&self->calib, sizeof(self->calib), 1, debug);
			fflush(debug);
#endif

			data.type = INFO;
			data.data.info.seq = IMS100FrameOdd_seq(&self->odd);
			data.data.info.sonde_serial = "IMSPlacehold";

			/* Invalidate data if subframe is marked corrupted */
			if (data.data.info.seq == -1) {
				data.type = EMPTY;
			}

			self->state = PARSE_END;
			break;
		/* }}} */

		case PARSE_END:
			data.type = FRAME_END;
			self->state = READ;
			break;
		default:
			self->state = READ;
			break;
	}

	return data;
}

