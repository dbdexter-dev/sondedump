#include <include/ims100.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "bitops.h"
#include "decode/framer.h"
#include "decode/manchester.h"
#include "frame.h"
#include "protocol.h"
#include "subframe.h"
#include "utils.h"
#include "log/log.h"

#ifndef NDEBUG
static FILE *debug, *debug_odd;
#endif

struct ims100decoder {
	Framer f;
	RSDecoder rs;
	IMS100ECCFrame raw_frame[4];
	IMS100Frame frame;
	IMS100Calibration calib;
	size_t offset;

	uint64_t calib_bitmask;
	int state;
	char serial[16];

	struct {
		float alt;
		time_t time;
	} prev_alt, cur_alt;
	IMS100FrameADC adc;
};

enum { READ_PRE, READ, PARSE_INFO, PARSE_PTU,
	PARSE_GPS_TIME, PARSE_GPS_POS,
	PARSE_META_FRAGMENTS,
	PARSE_END };

static void update_calibration(IMS100Decoder *self, int seq, uint8_t *fragment);

IMS100Decoder*
ims100_decoder_init(int samplerate)
{
	IMS100Decoder *d = malloc(sizeof(*d));

	framer_init_gfsk(&d->f, samplerate, IMS100_BAUDRATE, IMS100_SYNCWORD, IMS100_SYNC_LEN);
	bch_init(&d->rs, IMS100_REEDSOLOMON_N, IMS100_REEDSOLOMON_K,
			IMS100_REEDSOLOMON_POLY, ims100_bch_roots, IMS100_REEDSOLOMON_T);

	d->offset = 0;
	d->state = READ;

	d->calib_bitmask = 0;
	d->prev_alt.alt = 0;
	d->prev_alt.time = 0;
	d->serial[0] = 0;
#ifndef NDEBUG
	debug = fopen("/tmp/ims100frames.data", "wb");
	debug_odd = fopen("/tmp/ims100frames_odd.data", "wb");
#endif

	return d;
}

void
ims100_decoder_deinit(IMS100Decoder *d)
{
	framer_deinit(&d->f);
	rs_deinit(&d->rs);
	free(d);
#ifndef NDEBUG
	if (debug) fclose(debug);
	if (debug_odd) fclose(debug_odd);
#endif
}

ParserStatus
ims100_decode(IMS100Decoder *self, SondeData *dst, const float *src, size_t len)
{
	uint8_t *const raw_frame = (uint8_t*)self->raw_frame;
	unsigned int seq;
	uint32_t validmask;

	dst->type = EMPTY;

	switch (self->state) {
	case READ_PRE:
		self->raw_frame[0] = self->raw_frame[2];
		self->raw_frame[1] = self->raw_frame[3];
		self->state = READ;
		/* FALLTHROUGH */
	case READ:
		/* Read a new frame */
		switch (framer_read(&self->f, raw_frame, &self->offset, IMS100_FRAME_LEN, src, len)) {
		case PROCEED:
			return PROCEED;
		case PARSED:
			break;
		}

#ifndef NDEBUG
		fwrite(&self->raw_frame, 2*sizeof(self->raw_frame), 1, debug);
#endif
		/* Decode bits and move them in the right place */
		manchester_decode(raw_frame, raw_frame, IMS100_FRAME_LEN);
		ims100_frame_descramble(self->raw_frame);

		/* Error correct and remove all ECC bits */
		if (ims100_frame_error_correct(self->raw_frame, &self->rs) < 0) {
			self->state = READ_PRE;
			return PARSED;
		}
		ims100_frame_unpack(&self->frame, self->raw_frame);
		self->state = PARSE_INFO;

		/* FALLTHROUGH */
	case PARSE_INFO:
		/* Copy calibration data */
		if (IMS100_DATA_VALID(self->frame.valid, IMS100_MASK_SEQ | IMS100_MASK_CALIB)) {
			seq = ims100_frame_seq(&self->frame);
			update_calibration(self, seq, self->frame.calib);
		}

		dst->type = INFO;

		/* Invalidate data if subframe is marked corrupted */
		validmask = IMS100_MASK_SEQ;
		if (!IMS100_DATA_VALID(self->frame.valid, validmask)) dst->type = EMPTY;
		if (IMS100_DATA_VALID(self->calib_bitmask, IMS100_CALIB_SERIAL_MASK)) {
			sprintf(self->serial, "IMS%d", (int)ieee754_be(self->calib.serial));
		}

		if (dst->type != EMPTY) {
			dst->data.info.seq = ims100_frame_seq(&self->frame);
			dst->data.info.sonde_serial = self->serial;
		}

		self->state = PARSE_PTU;
		break;

	case PARSE_PTU:
		dst->type = PTU;

		/* Invalidate data if subframe is marked corrupted */
		validmask = IMS100_MASK_PTU;
		if (!IMS100_DATA_VALID(self->frame.valid, validmask)) dst->type = EMPTY;

#if 0
//#ifndef NDEBUG
		if (debug && debug_odd) {
			static int offset[2] = {2, 2};
			int myseq = ims100_frame_seq(&self->frame);
			static IMS100Frame empty;
			if (myseq & 0x1) {
				if ((myseq & 0x2) == offset[0])
					fwrite(&empty, sizeof(empty), 1, debug);
				fwrite(&self->frame, sizeof(self->frame), 1, debug);
				offset[0] = myseq & 0x2;
			} else {
				if ((myseq & 0x2) == offset[1])
					fwrite(&empty, sizeof(empty), 1, debug_odd);
				fwrite(&self->frame, sizeof(self->frame), 1, debug_odd);
				offset[1] = myseq & 0x2;
			}
			fflush(debug);
		}
#endif
		if (dst->type != EMPTY) {
			/* Fetch the ADC data carried by this frame based on its seq nr */
			switch (ims100_frame_seq(&self->frame) & 0x3) {
			case 0x00:
				self->adc.ref = (uint16_t)self->frame.adc_val0[0] << 8 | self->frame.adc_val0[1];
				self->adc.temp = (uint16_t)self->frame.adc_val1[0] << 8 | self->frame.adc_val1[1];
				self->adc.rh = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
				break;
			case 0x01:
			case 0x02:
				self->adc.temp = (uint16_t)self->frame.adc_val1[0] << 8 | self->frame.adc_val1[1];
				self->adc.rh = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
				break;
			case 0x03:
				self->adc.rh_temp = (uint16_t)self->frame.adc_val0[0] << 8 | self->frame.adc_val0[1];
				self->adc.temp = (uint16_t)self->frame.adc_val1[0] << 8 | self->frame.adc_val1[1];
				self->adc.ref = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
				break;
			}

			/* Parse PTU data */
			dst->data.ptu.calib_percent = 100.0 * count_ones((uint8_t*)&self->calib_bitmask, sizeof(self->calib_bitmask)) / IMS100_CALIB_FRAGCOUNT;
			dst->data.ptu.calibrated = IMS100_DATA_VALID(self->calib_bitmask, IMS100_CALIB_PTU_MASK);
			dst->data.ptu.temp = ims100_frame_temp(&self->adc, &self->calib);
			dst->data.ptu.rh = ims100_frame_rh(&self->adc, &self->calib);
			dst->data.ptu.pressure = 0;
		}

		switch (ims100_frame_subtype(&self->frame)) {
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
		dst->type = DATETIME;

		/* Invalidate data if subframe is marked corrupted */
		validmask = IMS100_GPS_MASK_TIME | IMS100_GPS_MASK_DATE;
		if (!IMS100_DATA_VALID(self->frame.valid, validmask)) dst->type = EMPTY;

		if (dst->type != EMPTY) {
			dst->data.datetime.datetime = ims100_subframe_time(&self->frame.data.gps);
			self->cur_alt.time = dst->data.datetime.datetime;
		}

		self->state = PARSE_GPS_POS;
		break;
	case PARSE_GPS_POS:
		dst->type = POSITION;

		/* Invalidate data if subframe is marked corrupted */
		validmask = IMS100_GPS_MASK_LAT | IMS100_GPS_MASK_LON | IMS100_GPS_MASK_ALT
				  | IMS100_GPS_MASK_SPEED | IMS100_GPS_MASK_HEADING;
		if (!IMS100_DATA_VALID(self->frame.valid, validmask)) dst->type = EMPTY;

		if (dst->type != EMPTY) {
			dst->data.pos.lat = ims100_subframe_lat(&self->frame.data.gps);
			dst->data.pos.lon = ims100_subframe_lon(&self->frame.data.gps);
			dst->data.pos.alt = ims100_subframe_alt(&self->frame.data.gps);
			dst->data.pos.speed = ims100_subframe_speed(&self->frame.data.gps);
			dst->data.pos.heading = ims100_subframe_heading(&self->frame.data.gps);
			self->cur_alt.alt = dst->data.pos.alt;

			/* Derive climb rate from altitude */
			if (self->cur_alt.time > self->prev_alt.time) {
				dst->data.pos.climb = (self->cur_alt.alt - self->prev_alt.alt)
									/ (self->cur_alt.time - self->prev_alt.time);
			}
			self->prev_alt = self->cur_alt;
		}

		self->state = PARSE_END;
		break;
	/* }}} */
	/* Other subframe parsing (meta? idk) {{{*/
	case PARSE_META_FRAGMENTS:
		self->state = PARSE_END;
		break;
	/* }}} */

	case PARSE_END:
		dst->type = FRAME_END;
		self->state = READ_PRE;
		break;
	default:
		self->state = READ;
		break;
	}

	return PARSED;
}

static void
update_calibration(IMS100Decoder *self, int seq, uint8_t *fragment)
{
	const int calib_offset = seq % IMS100_CALIB_FRAGCOUNT;
	memcpy(((uint8_t*)&self->calib) + IMS100_CALIB_FRAGSIZE * calib_offset, fragment + 2, 2);
	memcpy(((uint8_t*)&self->calib) + IMS100_CALIB_FRAGSIZE * calib_offset + 2, fragment, 2);

	self->calib_bitmask |= (1ULL << (63 - calib_offset));
}
