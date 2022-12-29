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
	IMS100ECCFrame ecc_frame;
	IMS100Frame frame;
	IMS100Calibration calib;

	uint64_t calib_bitmask;
	char serial[16];

	struct {
		float alt;
		time_t time;
	} prev_alt, cur_alt;
	IMS100FrameADC adc;
};

static void update_calibration(IMS100Decoder *self, int seq, uint8_t *fragment);

IMS100Decoder*
ims100_decoder_init(int samplerate)
{
	IMS100Decoder *d = malloc(sizeof(*d));

	framer_init_gfsk(&d->f, samplerate, IMS100_BAUDRATE, IMS100_FRAME_LEN, IMS100_SYNCWORD, IMS100_SYNC_LEN);
	bch_init(&d->rs, IMS100_REEDSOLOMON_N, IMS100_REEDSOLOMON_K,
			IMS100_REEDSOLOMON_POLY, ims100_bch_roots, IMS100_REEDSOLOMON_T);

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
	unsigned int seq;
	uint32_t validmask;

	/* Read a new frame */
	switch (framer_read(&self->f, self->raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}
#ifndef NDEBUG
	fwrite(raw_frame, 2*sizeof(self->ecc_frame), 1, debug);
#endif


	/* Decode bits and move them in the right place */
	manchester_decode(&self->ecc_frame, self->raw_frame, IMS100_FRAME_LEN);
	ims100_frame_descramble(&self->ecc_frame);

	/* Prepare for subframe parsing */
	dst->fields = 0;

	/* Error correct and remove all ECC bits */
	if (ims100_frame_error_correct(&self->ecc_frame, &self->rs) < 0) {
		/* ECC failed: go to next frame */
		return PARSED;
	}
	ims100_frame_unpack(&self->frame, &self->ecc_frame);

	/* Copy calibration data */
	if (BITMASK_CHECK(self->frame.valid, IMS100_MASK_SEQ | IMS100_MASK_CALIB)) {
		seq = ims100_frame_seq(&self->frame);
		update_calibration(self, seq, self->frame.calib);
	}

	/* Parse seq and serial number */
	if (BITMASK_CHECK(self->frame.valid, IMS100_MASK_SEQ)) {
		dst->fields |= DATA_SEQ;
		dst->seq = ims100_frame_seq(&self->frame);
	}

	/* Parse PTU data {{{ */
	if (BITMASK_CHECK(self->frame.valid, IMS100_MASK_PTU)) {
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

		dst->fields |= DATA_PTU;
		dst->temp = ims100_frame_temp(&self->adc, &self->calib);
		dst->rh = ims100_frame_rh(&self->adc, &self->calib);
		dst->pressure = 0;
		dst->calib_percent = 100.0 * count_ones((uint8_t*)&self->calib_bitmask,
												sizeof(self->calib_bitmask))
						   / IMS100_CALIB_FRAGCOUNT;
	}
	/* }}} */
	/* Parse subtype-specific data {{{ */
	switch (ims100_frame_subtype(&self->frame)) {
	case IMS100_SUBTYPE_GPS:
		/* Parse GPS time */
		validmask = IMS100_GPS_MASK_TIME | IMS100_GPS_MASK_DATE;
		if (BITMASK_CHECK(self->frame.valid, validmask)) {
			dst->fields |= DATA_TIME;
			dst->time = ims100_subframe_time(&self->frame.data.gps);
			self->cur_alt.time = dst->time;
		}

		/* Parse GPS position */
		validmask = IMS100_GPS_MASK_LAT | IMS100_GPS_MASK_LON | IMS100_GPS_MASK_ALT;
		if (BITMASK_CHECK(self->frame.valid, validmask)) {
			dst->fields |= DATA_POS;
			dst->lat = ims100_subframe_lat(&self->frame.data.gps);
			dst->lon = ims100_subframe_lon(&self->frame.data.gps);
			dst->alt = ims100_subframe_alt(&self->frame.data.gps);
		}

		/* Parse GPS speed */
		validmask = IMS100_GPS_MASK_SPEED | IMS100_GPS_MASK_HEADING;
		if (BITMASK_CHECK(self->frame.valid, validmask)) {
			dst->fields |= DATA_SPEED;
			dst->speed = ims100_subframe_speed(&self->frame.data.gps);
			dst->heading = ims100_subframe_heading(&self->frame.data.gps);
			self->cur_alt.alt = dst->alt;

			/* Derive climb rate from altitude */
			if (self->cur_alt.time > self->prev_alt.time) {
				dst->climb = (self->cur_alt.alt - self->prev_alt.alt)
						   / (self->cur_alt.time - self->prev_alt.time);
			}
			self->prev_alt = self->cur_alt;
		}
		break;
	case IMS100_SUBTYPE_META:
		/* TODO any interesting data here? */
		break;
	default:
		break;
	}
	/* }}} */

	/* Parse serial number, only if at least one field has been parsed */
	if (dst->fields && BITMASK_CHECK(self->calib_bitmask, IMS100_CALIB_SERIAL_MASK)) {
		sprintf(self->serial, "IMS%d", (int)ieee754_be(self->calib.serial));
		dst->fields |= DATA_SERIAL;
		strncpy(dst->serial, self->serial, sizeof(dst->serial) - 1);
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
