#include <include/imet4.h>
#include <math.h>
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
#include "protocol.h"
#include "subframe.h"

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

	IMET4Subframe *subframe;

	/* Read a new frame */
	switch(framer_read(&self->f, self->raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}

	/* Reformat data inside the frame */
	imet4_frame_descramble(&self->frame, self->raw_frame);

#ifndef NDEBUG
	if (debug) fwrite(self->raw_frame, IMET4_FRAME_LEN/8, 1, debug);
#endif

	/* Prepare to parse subframes */
	dst->fields = 0;

	for (i = 0; i < sizeof(self->frame.data); i += imet4_subframe_len(subframe)) {
		/* Extract subframe */
		subframe = (IMET4Subframe*)&self->frame.data[i];

		/* If zero-length, stop parsing the entire frame */
		if (!imet4_subframe_len(subframe)) break;

		/* Verify CRC, and invalidate frame if it doesn't match */
		if (!crc16_aug_ccitt((uint8_t*)subframe, imet4_subframe_len(subframe))) {
			/* Parse subframe */
			imet4_parse_subframe(dst, subframe);
		}
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

	int32_t pressure;
	int hour, min, sec;
	time_t now;
	struct tm datetime;

	switch (subframe->type) {
	case IMET4_SFTYPE_PTU:
	case IMET4_SFTYPE_PTUX:
		/* PTUX has the same fields as PTU, plus some extra that we are not
		 * parsing at the moment. To avoid code duplication, "downgrade"
		 * PTUX packets to PTU */
		pressure = ptu->pressure[0] | ptu->pressure[1] << 8 | ptu->pressure[2] << 16;
		pressure = (pressure << 8) >> 8;

		dst->fields |= DATA_PTU;
		dst->calib_percent = 100.0;
		dst->temp = ptu->temp / 100.0;
		dst->rh = ptu->rh / 100.0;
		dst->pressure = pressure / 100.0;

		dst->fields |= DATA_SEQ;
		dst->seq = ptu->seq;
		break;

	case IMET4_SFTYPE_GPS:
	case IMET4_SFTYPE_GPSX:
		/* Same reasoning as for PTU and PTUX: lat/lon/alt fields are
		 * shared, so avoid code duplication by combining them */
		dst->fields |= DATA_POS;
		dst->lat = gps->lat;
		dst->lon = gps->lon;
		dst->alt = gps->alt - 5000.0;

		/* Get time from the correct field, based on frame type */
		switch (subframe->type) {
		case IMET4_SFTYPE_GPS:
			hour = gps->hour;
			min = gps->min;
			sec = gps->sec;
			break;

			break;
		case IMET4_SFTYPE_GPSX:
			hour = gpsx->hour;
			min = gpsx->min;
			sec = gpsx->sec;

			dst->fields |= DATA_SPEED;
			dst->speed = sqrtf(gpsx->dlon*gpsx->dlon + gpsx->dlat*gpsx->dlat);
			dst->climb = gpsx->climb;
			dst->heading = atan2f(gpsx->dlat, gpsx->dlon) * 180.0 / M_PI;
			if (dst->heading < 0) dst->heading += 360.0;
			break;
		default:
			/* unreached */
			hour = min = sec = 0;
			break;
		}

		/* Date is not transmitted: use current date */
		now = time(NULL);
		datetime = *gmtime(&now);
		/* Handle 0Z crossing */
		if (abs(hour - datetime.tm_hour) >= 12) {
			now += (hour < datetime.tm_hour) ? 86400 : -86400;
			datetime = *gmtime(&now);
		}

		datetime.tm_hour = hour;
		datetime.tm_min = min;
		datetime.tm_sec = sec;

		dst->fields |= DATA_TIME;
		dst->time = my_timegm(&datetime);
		break;
	case IMET4_SFTYPE_XDATA:
		switch (xdata->instr_id) {
		case IMET4_XDATA_INSTR_OZONE:
			ozone_xdata = (IMET4Subframe_XDATA_Ozone*)(&xdata->data);
			dst->fields |= DATA_XDATA;
			dst->xdata.o3_ppb = imet4_subframe_xdata_ozone(dst->pressure, ozone_xdata);
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
