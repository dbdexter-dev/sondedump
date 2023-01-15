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

static void ims100_parse_frame(IMS100Decoder *self, SondeData *dst);
static void rs11g_parse_frame(IMS100Decoder *self, SondeData *dst);

static void ims100_update_calibration(IMS100Decoder *self, int seq, const uint8_t *fragment);
static void rs11g_update_calibration(IMS100Decoder *self, int seq, const uint8_t *fragment);

struct ims100decoder {
	Framer f;
	RSDecoder rs;
	IMS100ECCFrame raw_frame[4];
	IMS100ECCFrame ecc_frame;
	IMS100Frame frame;

	union {
		IMS100Calibration ims100;
		RS11GCalibration rs11g;
	} calib;

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
	fwrite(&self->calib.ims100, sizeof(self->calib.ims100), 1, debug);
	fflush(debug);
#endif

	switch (self->frame.subtype) {
	case SUBTYPE_RS11G:
		rs11g_parse_frame(self, dst);
		break;
	case SUBTYPE_IMS100:
		ims100_parse_frame(self, dst);
		break;
	}

	return PARSED;
}

/* Static functions {{{ */
static void
ims100_parse_frame(IMS100Decoder *self, SondeData *dst)
{
	uint32_t validmask;

	/* Parse seq */
	validmask = IMS100_MASK_SEQ;
	if (BITMASK_CHECK(self->frame.valid, validmask)) {
		dst->fields |= DATA_SEQ;
		dst->seq = ims100_frame_seq(&self->frame);
	}

	/* Update calibration data */
	validmask = IMS100_MASK_SEQ | IMS100_MASK_CALIB;
	if (BITMASK_CHECK(self->frame.valid, validmask)) {
		ims100_update_calibration(self, dst->seq, self->frame.calib);
	}

	/* Parse ADC data */
	validmask = IMS100_MASK_PTU;
	if (BITMASK_CHECK(self->frame.valid, validmask)) {
		self->adc.temp = (uint16_t)self->frame.adc_val1[0] << 8 | self->frame.adc_val1[1];

		/* Some ADC values depend on the sequence number */
		switch (ims100_frame_seq(&self->frame) & 0x3) {
		case 0x00:
			self->adc.ref = (uint16_t)self->frame.adc_val0[0] << 8 | self->frame.adc_val0[1];
			self->adc.rh = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
			break;
		case 0x01:
			self->adc.rh = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
			break;
		case 0x02:
			self->adc.rh = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
			break;
		case 0x03:
			self->adc.rh_temp = (uint16_t)self->frame.adc_val0[0] << 8 | self->frame.adc_val0[1];
			self->adc.ref = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
			break;
		}

		dst->fields |= DATA_PTU;
		dst->temp = ims100_frame_temp(&self->adc, &self->calib.ims100);
		dst->rh = ims100_frame_rh(&self->adc, &self->calib.ims100);
		dst->pressure = 0;
		dst->calib_percent = 100.0 * count_ones((uint8_t*)&self->calib_bitmask,
		                                        sizeof(self->calib_bitmask))
		                  / IMS100_CALIB_FRAGCOUNT;
	}


	/* Parse the rest of the frame */
	switch (self->frame.subseq << 8 | self->frame.subtype) {
	case IMS100_SUBTYPE_GPS:
		/* Parse GPS time */
		validmask = IMS100_GPS_MASK_TIME | IMS100_GPS_MASK_DATE;
		if (BITMASK_CHECK(self->frame.valid, validmask)) {
			dst->fields |= DATA_TIME;
			dst->time = ims100_subframe_time(&self->frame.data.gps);
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
			dst->climb = NAN;
		}
		break;

	case IMS100_SUBTYPE_META:
		/* TODO any interesting data here? */
		break;
	}

	/* Derive climb rate from altitude data */
	if (BITMASK_CHECK(dst->fields, DATA_POS | DATA_TIME)) {
		self->cur_alt.alt = dst->alt;
		self->cur_alt.time = dst->time;

		if (self->cur_alt.time > self->prev_alt.time) {
			dst->climb = (self->cur_alt.alt - self->prev_alt.alt)
			           / (self->cur_alt.time - self->prev_alt.time);
		}

		self->prev_alt = self->cur_alt;
	}

	/* Append serial number */
	if (dst->fields && BITMASK_CHECK(self->calib_bitmask, IMS100_CALIB_SERIAL_MASK)) {
		dst->fields |= DATA_SERIAL;
		sprintf(dst->serial, "IMS%d", (int)self->calib.ims100.serial);
	}
}

static void
rs11g_parse_frame(IMS100Decoder *self, SondeData *dst)
{
	uint32_t validmask;

	/* Parse seq */
	validmask = IMS100_MASK_SEQ;
	if (BITMASK_CHECK(self->frame.valid, validmask)) {
		dst->fields |= DATA_SEQ;
		dst->seq = ims100_frame_seq(&self->frame);
	}

	/* Update calibration data */
	validmask = IMS100_MASK_SEQ | IMS100_MASK_CALIB;
	if (BITMASK_CHECK(self->frame.valid, validmask)) {
		rs11g_update_calibration(self, dst->seq, self->frame.calib);
	}

	/* Parse ADC data */
	validmask = IMS100_MASK_PTU;
	if (BITMASK_CHECK(self->frame.valid, validmask)) {
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

		dst->fields |= DATA_PTU;
		dst->temp = rs11g_frame_temp(&self->adc, &self->calib.rs11g);
		dst->rh = rs11g_frame_rh(&self->adc, &self->calib.rs11g);
		dst->pressure = 0;
		dst->calib_percent = 100.0 * count_ones((uint8_t*)&self->calib_bitmask,
		                                        sizeof(self->calib_bitmask))
		                  / IMS100_CALIB_FRAGCOUNT;
	}

	/* Parse the rest of the frame */
	switch (self->frame.subseq << 8 | self->frame.subtype) {
	case RS11G_SUBTYPE_GPS:
		/* Parse GPS position */
		validmask = RS11G_GPS_MASK_LAT | RS11G_GPS_MASK_LON | RS11G_GPS_MASK_ALT;
		if (BITMASK_CHECK(self->frame.valid, validmask)) {
			dst->fields |= DATA_POS;
			dst->lat = rs11g_subframe_lat(&self->frame.data.gps_11g);
			dst->lon = rs11g_subframe_lon(&self->frame.data.gps_11g);
			dst->alt = rs11g_subframe_alt(&self->frame.data.gps_11g);
		}

		/* Parse GPS speed */
		validmask = RS11G_GPS_MASK_SPEED | RS11G_GPS_MASK_HEADING | RS11G_GPS_MASK_CLIMB;
		if (BITMASK_CHECK(self->frame.valid, validmask)) {
			dst->fields |= DATA_SPEED;
			dst->speed = rs11g_subframe_speed(&self->frame.data.gps_11g);
			dst->heading = rs11g_subframe_heading(&self->frame.data.gps_11g);
			dst->climb = rs11g_subframe_climb(&self->frame.data.gps_11g);
		}

		/* Parse GPS time (1/2) */
		validmask = RS11G_GPS_MASK_DATE;
		if (BITMASK_CHECK(self->frame.valid, validmask)) {
			self->date = rs11g_subframe_date(&self->frame.data.gps_11g);
		}
		break;

	case RS11G_SUBTYPE_GPSRAW:
		/* Parse GPS time (2/2) */
		validmask = RS11G_GPSRAW_MASK_TIME;
		if (BITMASK_CHECK(self->frame.valid, validmask)) {
			dst->fields |= DATA_TIME;
			dst->time = self->date + rs11g_subframe_time(&self->frame.data.gpsraw_11g);
		}
		break;
	default:
		break;
	}

	/* Append serial number */
	if (dst->fields && BITMASK_CHECK(self->calib_bitmask, IMS100_CALIB_SERIAL_MASK)) {
		dst->fields |= DATA_SERIAL;
		sprintf(dst->serial, "RS11G-%d", (int)self->calib.rs11g.serial);
	}
}

static void
ims100_update_calibration(IMS100Decoder *self, int seq, const uint8_t *fragment)
{
	const int calib_offset = seq % IMS100_CALIB_FRAGCOUNT;
	const uint8_t raw[] = {fragment[2], fragment[3], fragment[0], fragment[1]};
	const float coeff = ieee754_be(raw);

	memcpy(((uint8_t*)&self->calib.ims100) + IMS100_CALIB_FRAGSIZE * calib_offset, &coeff, sizeof(coeff));

	self->calib_bitmask |= (1ULL << (63 - calib_offset));
}

static void
rs11g_update_calibration(IMS100Decoder *self, int seq, const uint8_t *fragment)
{
	const int calib_offset = seq % IMS100_CALIB_FRAGCOUNT;
	const float coeff = mbf_le(fragment);

	memcpy(((uint8_t*)&self->calib.rs11g) + IMS100_CALIB_FRAGSIZE * calib_offset, &coeff, sizeof(coeff));

	self->calib_bitmask |= (1ULL << (63 - calib_offset));
}
/* }}} */
