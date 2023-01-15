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

static void ims100_parse_subframe(SondeData *dst, time_t *date, const IMS100Frame *frame);
static void ims100_update_calibration(IMS100Decoder *self, int seq, const uint8_t *fragment);
static void rs11g_update_calibration(IMS100Decoder *self, int seq, const uint8_t *fragment);

struct ims100decoder {
	Framer f;
	RSDecoder rs;
	IMS100ECCFrame raw_frame[4];
	IMS100ECCFrame ecc_frame;
	IMS100Frame frame;

	IMS100Calibration calib;
	RS11GCalibration calib_rs11g;

	uint64_t calib_bitmask;
	time_t date;

	struct {
		float alt;
		time_t time;
	} prev_alt, cur_alt;
	IMS100FrameADC adc;
};


__global IMS100Decoder*
ims100_decoder_init(int samplerate)
{
	IMS100Decoder *d = malloc(sizeof(*d));

	framer_init_gfsk(&d->f, samplerate, IMS100_BAUDRATE, IMS100_FRAME_LEN, IMS100_SYNCWORD, IMS100_SYNC_LEN);
	bch_init(&d->rs, IMS100_REEDSOLOMON_N, IMS100_REEDSOLOMON_K,
			IMS100_REEDSOLOMON_POLY, ims100_bch_roots, IMS100_REEDSOLOMON_T);

	d->calib_bitmask = 0;
	d->prev_alt.alt = 0;
	d->prev_alt.time = -1UL;
	d->date = 0;
#ifndef NDEBUG
	debug = fopen("/tmp/ims100frames.data", "wb");
	debug_odd = fopen("/tmp/ims100frames_odd.data", "wb");
#endif

	return d;
}

__global void
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

__global ParserStatus
ims100_decode(IMS100Decoder *self, SondeData *dst, const float *src, size_t len)
{
	unsigned int seq;
	int errcount;

	/* Read a new frame */
	switch (framer_read(&self->f, self->raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}


	/* Decode bits and move them in the right place */
	manchester_decode(&self->ecc_frame, self->raw_frame, IMS100_FRAME_LEN);
	ims100_frame_descramble(&self->ecc_frame);

	/* Prepare for subframe parsing */
	dst->fields = 0;

	/* Error correct and remove all ECC bits */
	errcount = ims100_frame_error_correct(&self->ecc_frame, &self->rs);
	log_debug("Error count: %d", errcount);
	if (errcount < 0) {
		/* ECC failed: go to next frame */
		return PARSED;
	}
	ims100_frame_unpack(&self->frame, &self->ecc_frame);

#ifndef NDEBUG
	fwrite(&self->calib_rs11g, sizeof(self->calib_rs11g), 1, debug);
	fflush(debug);
#endif



	/* Copy calibration data */
	if (BITMASK_CHECK(self->frame.valid, IMS100_MASK_SEQ | IMS100_MASK_CALIB)) {
		seq = ims100_frame_seq(&self->frame);

		switch (self->frame.subtype) {
		case SUBTYPE_IMS100:
			ims100_update_calibration(self, seq, self->frame.calib);
			break;
		case SUBTYPE_RS11G:
			rs11g_update_calibration(self, seq, self->frame.calib);
			break;

		}
	}

	/* Parse seq and serial number */
	if (BITMASK_CHECK(self->frame.valid, IMS100_MASK_SEQ)) {
		dst->fields |= DATA_SEQ;
		dst->seq = ims100_frame_seq(&self->frame);
	}

	/* Parse common PTU data {{{ */
	if (BITMASK_CHECK(self->frame.valid, IMS100_MASK_PTU)) {
		/* Fetch the ADC data carried by this frame based on its seq nr */
		switch (self->frame.subtype) {
		case SUBTYPE_IMS100:
			switch (ims100_frame_seq(&self->frame) & 0x3) {
			case 0x00:
				self->adc.ref = (uint16_t)self->frame.adc_val0[0] << 8 | self->frame.adc_val0[1];
				self->adc.temp = (uint16_t)self->frame.adc_val1[0] << 8 | self->frame.adc_val1[1];
				self->adc.rh = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
				break;
			case 0x01:
				self->adc.temp = (uint16_t)self->frame.adc_val1[0] << 8 | self->frame.adc_val1[1];
				self->adc.rh = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
				break;
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
			break;
		case SUBTYPE_RS11G:
			switch (ims100_frame_seq(&self->frame) & 0x3) {
			case 0x00:
				self->adc.ref = (uint16_t)self->frame.adc_val0[0] << 8 | self->frame.adc_val0[1];
				break;
			case 0x01:
				break;
			case 0x02:
				break;
			case 0x03:
				break;
			}

			self->adc.temp = (uint16_t)self->frame.adc_val1[0] << 8 | self->frame.adc_val1[1];
			self->adc.rh = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
			break;
		}


		dst->fields |= DATA_PTU;

		switch (self->frame.subtype) {
		case SUBTYPE_IMS100:
			dst->temp = ims100_frame_temp(&self->adc, &self->calib);
			dst->rh = ims100_frame_rh(&self->adc, &self->calib);
			dst->pressure = 0;
			break;
		case SUBTYPE_RS11G:
			dst->temp = rs11g_frame_temp(&self->adc, &self->calib_rs11g);
			dst->rh = rs11g_frame_rh(&self->adc, &self->calib_rs11g);
			dst->pressure = 0;
			break;
		default:
			dst->fields &= ~DATA_PTU;
			break;
		}

		dst->calib_percent = 100.0 * count_ones((uint8_t*)&self->calib_bitmask,
												sizeof(self->calib_bitmask))
						   / IMS100_CALIB_FRAGCOUNT;
	}
	/* }}} */

	/* Parse subframe-specific data */
	ims100_parse_subframe(dst, &self->date, &self->frame);

	/* If speed data is missing, derive it from altitude data */
	if (BITMASK_CHECK(dst->fields, DATA_POS | DATA_TIME)) {
		self->cur_alt.alt = dst->alt;
		self->cur_alt.time = dst->time;

		if (self->cur_alt.time > self->prev_alt.time && isnan(dst->climb)) {
			dst->climb = (self->cur_alt.alt - self->prev_alt.alt)
			           / (self->cur_alt.time - self->prev_alt.time);
		}

		self->prev_alt = self->cur_alt;
	}

	/* Return serial number only if at least one field has been parsed */
	if (dst->fields && BITMASK_CHECK(self->calib_bitmask, IMS100_CALIB_SERIAL_MASK)) {
		switch (self->frame.subtype) {
		case SUBTYPE_IMS100:
			sprintf(dst->serial, "IMS%d", (int)self->calib.serial);
			break;

		case SUBTYPE_RS11G:
			sprintf(dst->serial, "RS11G-%d", (int)self->calib_rs11g.serial);
			break;

		default:
			break;
		}

		dst->fields |= DATA_SERIAL;
	}

	return PARSED;
}

/* Static functions {{{ */
static void
ims100_parse_subframe(SondeData *dst, time_t *date, const IMS100Frame *frame)
{
	uint32_t validmask;

	/* Parse subtype-specific data {{{ */
	switch (frame->subseq << 8 | frame->subtype) {
	case IMS100_SUBTYPE_GPS:
		/* Parse GPS time */
		validmask = IMS100_GPS_MASK_TIME | IMS100_GPS_MASK_DATE;
		if (BITMASK_CHECK(frame->valid, validmask)) {
			dst->fields |= DATA_TIME;
			dst->time = ims100_subframe_time(&frame->data.gps);
		}

		/* Parse GPS position */
		validmask = IMS100_GPS_MASK_LAT | IMS100_GPS_MASK_LON | IMS100_GPS_MASK_ALT;
		if (BITMASK_CHECK(frame->valid, validmask)) {
			dst->fields |= DATA_POS;
			dst->lat = ims100_subframe_lat(&frame->data.gps);
			dst->lon = ims100_subframe_lon(&frame->data.gps);
			dst->alt = ims100_subframe_alt(&frame->data.gps);
		}

		/* Parse GPS speed */
		validmask = IMS100_GPS_MASK_SPEED | IMS100_GPS_MASK_HEADING;
		if (BITMASK_CHECK(frame->valid, validmask)) {
			dst->fields |= DATA_SPEED;
			dst->speed = ims100_subframe_speed(&frame->data.gps);
			dst->heading = ims100_subframe_heading(&frame->data.gps);
			dst->climb = NAN;
		}
		break;

	case IMS100_SUBTYPE_META:
		/* TODO any interesting data here? */
		break;

	case RS11G_SUBTYPE_GPS:
		/* Parse GPS position */
		validmask = RS11G_GPS_MASK_LAT | RS11G_GPS_MASK_LON | RS11G_GPS_MASK_ALT;
		if (BITMASK_CHECK(frame->valid, validmask)) {
			dst->fields |= DATA_POS;
			dst->lat = rs11g_subframe_lat(&frame->data.gps_11g);
			dst->lon = rs11g_subframe_lon(&frame->data.gps_11g);
			dst->alt = rs11g_subframe_alt(&frame->data.gps_11g);
		}

		/* Parse GPS speed */
		validmask = RS11G_GPS_MASK_SPEED | RS11G_GPS_MASK_HEADING | RS11G_GPS_MASK_CLIMB;
		if (BITMASK_CHECK(frame->valid, validmask)) {
			dst->fields |= DATA_SPEED;
			dst->speed = rs11g_subframe_speed(&frame->data.gps_11g);
			dst->heading = rs11g_subframe_heading(&frame->data.gps_11g);
			dst->climb = rs11g_subframe_climb(&frame->data.gps_11g);
		}

		/* Parse GPS time (1/2) */
		validmask = RS11G_GPS_MASK_DATE;
		if (BITMASK_CHECK(frame->valid, validmask)) {
			*date = rs11g_subframe_date(&frame->data.gps_11g);
			log_debug("%u", *date);
		}
		break;

	case RS11G_SUBTYPE_GPSRAW:
		/* Parse GPS time (2/2) */
		validmask = RS11G_GPSRAW_MASK_TIME;
		if (BITMASK_CHECK(frame->valid, validmask)) {
			dst->fields |= DATA_TIME;
			dst->time = *date + rs11g_subframe_time(&frame->data.gpsraw_11g);
		}
		break;
	default:
		break;
	}
	/* }}} */
}

static void
ims100_update_calibration(IMS100Decoder *self, int seq, const uint8_t *fragment)
{
	const int calib_offset = seq % IMS100_CALIB_FRAGCOUNT;
	const uint8_t raw[] = {fragment[2], fragment[3], fragment[0], fragment[1]};
	const float coeff = ieee754_be(raw);

	memcpy(((uint8_t*)&self->calib) + IMS100_CALIB_FRAGSIZE * calib_offset, &coeff, sizeof(coeff));

	self->calib_bitmask |= (1ULL << (63 - calib_offset));
}

static void
rs11g_update_calibration(IMS100Decoder *self, int seq, const uint8_t *fragment)
{
	const int calib_offset = seq % IMS100_CALIB_FRAGCOUNT;
	const float coeff = mbf_le(fragment);

	memcpy(((uint8_t*)&self->calib_rs11g) + IMS100_CALIB_FRAGSIZE * calib_offset, &coeff, sizeof(coeff));

	self->calib_bitmask |= (1ULL << (63 - calib_offset));
}
/* }}} */
