#include <include/imet4.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bitops.h"
#include "decode/ecc/crc.h"
#include "decode/framer.h"
#include "decode/manchester.h"
#include "frame.h"
#include "gps/ecef.h"
#include "log/log.h"
#include "subframe.h"
#include "protocol.h"
#include "physics.h"
#include "parser.h"


static uint16_t imet4_serial(int seq, time_t time);
static void imet4_parse_subframe(SondeData *dst, IMET4Subframe *subframe);

struct imet4decoder {
	Framer f;
	IMET4Frame raw_frame[2];
	IMET4Frame frame;
	uint32_t prev_time;
	float prev_x, prev_y, prev_z;
};

#ifndef NDEBUG
static FILE *debug;
#endif


__global IMET4Decoder*
imet4_decoder_init(int samplerate)
{
	IMET4Decoder *d = malloc(sizeof(*d));
	framer_init_afsk(&d->f, samplerate, IMET4_BAUDRATE, IMET4_FRAME_LEN,
			IMET4_MARK_FREQ, IMET4_SPACE_FREQ,
			IMET4_SYNCWORD, IMET4_SYNC_LEN);

#ifndef NDEBUG
	debug = fopen("/tmp/imet4frames.data", "wb");
#endif

	return d;
}

__global void
imet4_decoder_deinit(IMET4Decoder *d) {
	framer_deinit(&d->f);
	free(d);

#ifndef NDEBUG
	if (debug) fclose(debug);
#endif

}

__global ParserStatus
imet4_decode(IMET4Decoder *self, SondeData *dst, const float *src, size_t len)
{
	float x, y, z, dt;
	size_t i;
	size_t bytes_consumed;
	int subframe_len;

	IMET4Subframe *subframe;

	/* Read a new frame */
	switch (framer_read(&self->f, self->raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}

	/* Reformat data inside the frame */
	imet4_frame_descramble(&self->frame, self->raw_frame);

#ifndef NDEBUG
	if (debug) fwrite(&self->frame, sizeof(self->frame), 1, debug);
#endif

	/* Prepare to parse subframes */
	dst->fields = 0;

	for (i = 0; i < sizeof(self->frame.data); i += subframe_len) {
		/* Extract subframe */
		subframe = (IMET4Subframe*)&self->frame.data[i];
		subframe_len = imet4_subframe_len(subframe);

		/* If zero-length, stop parsing the entire frame */
		if (!subframe_len) break;

		/* Verify CRC, and invalidate frame if it doesn't match */
		if (!crc16_aug_ccitt((uint8_t*)subframe, subframe_len)) {
			/* Parse subframe */
			imet4_parse_subframe(dst, subframe);
			log_debug("Type %d", subframe->type);
		} else if (subframe->type == IMET4_SFTYPE_XDATA) {
			/* Variable length packet corrupted: stop parsing */
			break;
		}
	}

	/* If anything was parsed, adjust the framer position by the actual number
	 * of bits used */
	if (i > 0) {
		bytes_consumed = offsetof(IMET4Frame, data) + i;
		framer_adjust(&self->f, self->raw_frame, 10 * bytes_consumed);  /* start + byte + stop */
		log_debug("Adjusting by %u bytes", bytes_consumed);
	}

	/* If speed data is missing, but both position and time data is available,
	 * compute it based on the position difference */
	if (!BITMASK_CHECK(dst->fields, DATA_SPEED) && BITMASK_CHECK(dst->fields, DATA_POS | DATA_TIME)) {
		dst->fields |= DATA_SPEED;

		/* Convert to ECEF coordinates to estimate speed vector */
		dt = dst->time - self->prev_time;
		lla_to_ecef(&x, &y, &z, dst->lat, dst->lon, dst->alt);
		ecef_to_spd_hdg(&dst->speed, &dst->heading, &dst->climb,
						dst->lat, dst->lon,
						(x - self->prev_x)/dt, (y - self->prev_y)/dt, (z - self->prev_z)/dt);

		/* Update last known x/y/z */
		self->prev_x = x;
		self->prev_y = y;
		self->prev_z = z;
		self->prev_time = dst->time;
	}

	/* Derive serial number from est. turn-on time */
	if (BITMASK_CHECK(dst->fields, DATA_SEQ | DATA_TIME)) {
		dst->fields |= DATA_SERIAL;
		sprintf(dst->serial, "iMet-%04X", imet4_serial(dst->seq, dst->time));
	}

	return PARSED;
}

/* Static functions {{{ */
static uint16_t
imet4_serial(int seq, time_t time)
{
	/* Roughly estimate the start-up time */
	time -= seq;

	return crc16_aug_ccitt((uint8_t*)&time, 4);
}

static void
imet4_parse_subframe(SondeData *dst, IMET4Subframe *subframe)
{
	IMET4Subframe_PTU *ptu = (IMET4Subframe_PTU*)subframe;
	IMET4Subframe_GPS *gps = (IMET4Subframe_GPS*)subframe;
	IMET4Subframe_GPSX *gpsx = (IMET4Subframe_GPSX*)subframe;
	IMET4Subframe_XDATA *xdata = (IMET4Subframe_XDATA*)subframe;

	IMET4Subframe_XDATA_Ozone *ozone_xdata;


	switch (subframe->type) {
	case IMET4_SFTYPE_PTU:
	case IMET4_SFTYPE_PTUX:
		/* PTUX has the same fields as PTU, plus some extra that we are not
		 * parsing at the moment. To avoid code duplication, "downgrade"
		 * PTUX packets to PTU */
		dst->calib_percent = 100.0;
		dst->temp = imet4_ptu_temp(ptu);
		dst->rh = imet4_ptu_rh(ptu);
		dst->pressure = imet4_ptu_pressure(ptu);
		dst->fields |= DATA_PTU;

		dst->seq = ptu->seq;
		dst->fields |= DATA_SEQ;
		break;

	case IMET4_SFTYPE_GPS:
		dst->lat = imet4_gps_lat(gps);
		dst->lon = imet4_gps_lon(gps);
		dst->alt = imet4_gps_alt(gps);
		dst->fields |= DATA_POS;

		dst->time = imet4_gps_time(gps);
		dst->fields |= DATA_TIME;
		break;

	case IMET4_SFTYPE_GPSX:
		dst->lat = imet4_gps_lat(gps);
		dst->lon = imet4_gps_lon(gps);
		dst->alt = imet4_gps_alt(gps);
		dst->fields |= DATA_POS;

		dst->time = imet4_gpsx_time(gpsx);
		dst->fields |= DATA_TIME;

		dst->speed = imet4_gpsx_speed(gpsx);
		dst->heading = imet4_gpsx_heading(gpsx);
		dst->climb = imet4_gpsx_climb(gpsx);
		dst->fields |= DATA_SPEED;
		break;

	case IMET4_SFTYPE_XDATA:
		switch (xdata->instr_id) {
		case IMET4_XDATA_INSTR_OZONE:
			/* If no pressure data was provided, derive it from the GPS altitude */
			ozone_xdata = (IMET4Subframe_XDATA_Ozone*)(&xdata->data);
			dst->fields |= DATA_OZONE;
			dst->o3_mpa = imet4_subframe_xdata_ozone(ozone_xdata);
			break;
		default:
			log_warn("Unknown XDATA instrument ID %02x", xdata->instr_id);
			break;
		}
		break;
	default:
		log_warn("Unknown subframe type 0x%x", subframe->type);
		break;
	}
}
/* }}} */
