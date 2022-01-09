#include <math.h>
#include <string.h>
#include "decode/framer.h"
#include "decode/manchester.h"
#include "frame.h"
#include "ims100.h"
#include "protocol.h"
#include "subframe.h"
#include "utils.h"

#ifndef NDEBUG
#include <stdio.h>
static FILE *debug, *debug_odd;
#endif

enum { READ, PARSE_INFO, PARSE_PTU,
	PARSE_GPS_TIME, PARSE_GPS_POS,
	PARSE_META_FRAGMENTS,
	PARSE_END };

static void update_calibration(IMS100Decoder *self, int seq, uint8_t *fragment);

void
ims100_decoder_init(IMS100Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, IMS100_BAUDRATE);
	correlator_init(&d->correlator, IMS100_SYNCWORD, IMS100_SYNC_LEN);
	bch_init(&d->rs, IMS100_REEDSOLOMON_N, IMS100_REEDSOLOMON_K,
			IMS100_REEDSOLOMON_POLY, ims100_bch_roots, IMS100_REEDSOLOMON_T);

	d->state = READ;
	d->calib_bitmask = 0;
	d->prev_alt.alt = 0;
	d->prev_alt.time = 0;
#ifndef NDEBUG
	debug = fopen("/tmp/ims100frames.data", "wb");
	debug_odd = fopen("/tmp/ims100frames_odd.data", "wb");
#endif
}

void
ims100_decoder_deinit(IMS100Decoder *d)
{
	gfsk_deinit(&d->gfsk);
#ifndef NDEBUG
	fclose(debug);
	fclose(debug_odd);
#endif
}

SondeData
ims100_decode(IMS100Decoder *self, int (*read)(float *dst))
{
	SondeData data = {.type = EMPTY};
	uint8_t *const raw_frame = (uint8_t*)self->raw_frame;
	unsigned int seq;
	uint32_t validmask;

	switch (self->state) {
		case READ:
			/* Read a new frame */
			if (read_frame_gfsk(&self->gfsk, &self->correlator, raw_frame, read, IMS100_FRAME_LEN, 0) < 0) {
				data.type = SOURCE_END;
				return data;
			}

			/* Decode bits and move them in the right palce */
			manchester_decode(raw_frame, raw_frame, IMS100_FRAME_LEN);
			ims100_frame_descramble(self->raw_frame);

			/* Error correct and remove all ECC bits */
			if (ims100_frame_error_correct(self->raw_frame, &self->rs) < 0) return data;
			ims100_frame_unpack(&self->frame, self->raw_frame);

			__attribute__((fallthrough));
		case PARSE_INFO:
			/* Copy calibration data */
			if (IMS100_DATA_VALID(self->frame.valid, IMS100_MASK_SEQ | IMS100_MASK_CALIB)) {
				seq = IMS100Frame_seq(&self->frame);
				update_calibration(self, seq, self->frame.calib);
			}

			data.type = INFO;

			/* Invalidate data if subframe is marked corrupted */
			validmask = IMS100_MASK_SEQ;
			if (!IMS100_DATA_VALID(self->frame.valid, validmask)) data.type = EMPTY;

			data.data.info.seq = IMS100Frame_seq(&self->frame);
			data.data.info.sonde_serial = "iMS100Placehold";
			self->state = PARSE_PTU;
			break;

		case PARSE_PTU:
			data.type = PTU;

			/* Invalidate data if subframe is marked corrupted */
			validmask = IMS100_MASK_PTU;
			if (!IMS100_DATA_VALID(self->frame.valid, validmask)) data.type = EMPTY;

			/* Parse PTU data */
			data.data.ptu.temp = IMS100Frame_temp(&self->frame, &self->calib);
			data.data.ptu.rh = IMS100Frame_rh(&self->frame, &self->calib);

			switch (IMS100Frame_subtype(&self->frame)) {
				case IMS100_SUBTYPE_GPS:
					self->state = PARSE_GPS_TIME;
					break;
				case IMS100_SUBTYPE_META:
					self->state = PARSE_META_FRAGMENTS;
					break;
				default:
					self->state = PARSE_END;
					break;
			}
			break;

		/* GPS subframe parsing {{{ */
		case PARSE_GPS_TIME:
			data.type = DATETIME;

			/* Invalidate data if subframe is marked corrupted */
			validmask = IMS100_GPS_MASK_TIME | IMS100_GPS_MASK_DATE;
			if (!IMS100_DATA_VALID(self->frame.valid, validmask)) data.type = EMPTY;

			data.data.datetime.datetime = IMS100FrameGPS_time(&self->frame.data.gps);
			self->cur_alt.time = data.data.datetime.datetime;

			self->state = PARSE_GPS_POS;
			break;
		case PARSE_GPS_POS:
			data.type = POSITION;

			/* Invalidate data if subframe is marked corrupted */
			validmask = IMS100_GPS_MASK_LAT | IMS100_GPS_MASK_LON | IMS100_GPS_MASK_ALT
			          | IMS100_GPS_MASK_SPEED | IMS100_GPS_MASK_HEADING;
			if (!IMS100_DATA_VALID(self->frame.valid, validmask)) data.type = EMPTY;

			data.data.pos.lat = IMS100FrameGPS_lat(&self->frame.data.gps);
			data.data.pos.lon = IMS100FrameGPS_lon(&self->frame.data.gps);
			data.data.pos.alt = IMS100FrameGPS_alt(&self->frame.data.gps);
			data.data.pos.speed = IMS100FrameGPS_speed(&self->frame.data.gps);
			data.data.pos.heading = IMS100FrameGPS_heading(&self->frame.data.gps);
			self->cur_alt.alt = data.data.pos.alt;

			/* Derive climb rate from altitude */
			if (self->cur_alt.time > self->prev_alt.time) {
				data.data.pos.climb = (self->cur_alt.alt - self->prev_alt.alt)
									/ (self->cur_alt.time - self->prev_alt.time);
			}
			self->prev_alt = self->cur_alt;


			self->state = PARSE_END;
			break;
		/* }}} */
		/* Other subframe parsing (meta? idk) {{{*/
		case PARSE_META_FRAGMENTS:
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

static void
update_calibration(IMS100Decoder *self, int seq, uint8_t *fragment)
{
	const int calib_offset = seq % IMS100_CALIB_FRAGCOUNT;
	memcpy(((uint8_t*)&self->calib) + IMS100_CALIB_FRAGSIZE * calib_offset, fragment, IMS100_CALIB_FRAGSIZE);

	self->calib_bitmask |= (1ULL << (63 - calib_offset));
}
