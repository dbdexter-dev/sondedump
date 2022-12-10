#include <include/imet4.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "decode/ecc/crc.h"
#include "decode/framer.h"
#include "decode/manchester.h"
#include "protocol.h"
#include "frame.h"
#include "gps/ecef.h"
#include "subframe.h"

static uint16_t imet4_serial(int seq, time_t time);

struct imet4decoder {
	Framer f;
	IMET4Frame frame[2];
	int state;
	uint32_t time;
	size_t offset, frame_offset;
	char serial[16];
	uint32_t prev_time;
	float prev_x, prev_y, prev_z;
};

enum { READ_PRE, READ, PARSE_SUBFRAME, PARSE_SUBFRAME_PTU_INFO, PARSE_SUBFRAME_GPS_POS };

#ifndef NDEBUG
static FILE *debug;
#endif


IMET4Decoder*
imet4_decoder_init(int samplerate)
{
	IMET4Decoder *d = malloc(sizeof(*d));
	framer_init_afsk(&d->f, samplerate, IMET4_BAUDRATE,
			IMET4_MARK_FREQ, IMET4_SPACE_FREQ,
			IMET4_SYNCWORD, IMET4_SYNC_LEN);

	d->state = READ;
	d->offset = 0;
	d->serial[0] = 0;
	d->time = 0;

#ifndef NDEBUG
	debug = fopen("/tmp/imet4frames.data", "wb");
#endif

	return d;
}

void
imet4_decoder_deinit(IMET4Decoder *d) {
	framer_deinit(&d->f);
	free(d);

#ifndef NDEBUG
	if (debug) fclose(debug);
#endif

}

ParserStatus
imet4_decode(IMET4Decoder *self, SondeData *dst, const float *src, size_t len)
{
	uint8_t *const raw_frame = (uint8_t*)self->frame;
	int32_t pressure;
	struct tm datetime;
	time_t now;
	int hour, min, sec;
	float x, y, z, dt;

	IMET4Subframe *subframe;
	IMET4Subframe_GPS *gps;
	IMET4Subframe_GPSX *gpsx;
	IMET4Subframe_PTU *ptu;
	//IMET4Subframe_PTUX *ptux;


	switch (self->state) {
	case READ_PRE:
		/* Copy residual bits from the previous frame */
		self->frame[0] = self->frame[1];
		self->state = READ;
		/* FALLTHROUGH */
	case READ:
		switch(framer_read(&self->f, raw_frame, &self->offset, IMET4_FRAME_LEN, src, len)) {
		case PROCEED:
			return PROCEED;
		case PARSED:
			break;
		}

		imet4_frame_descramble(self->frame);

#ifndef NDEBUG
		if (debug) fwrite(raw_frame, IMET4_FRAME_LEN/8, 1, debug);
#endif
		/* Prepare to parse subframes */
		memset(dst, 0, sizeof(*dst));
		self->frame_offset = 0;

		/* Extract the first subframe */
		subframe = (IMET4Subframe*)&self->frame->data[self->frame_offset];
		self->frame_offset += imet4_subframe_len(subframe);

		/* Continue until the end of the frame */
		while (imet4_subframe_len(subframe) &&
		       self->frame_offset < sizeof(self->frame->data)) {
			/* Validate the subframe's checksum against the one received. If it
			 * doesn't match, don't try to parse it */
			if (crc16_aug_ccitt((uint8_t*)subframe, imet4_subframe_len(subframe))) {
				subframe->type = 0x00;
			}

			/* Subframe parsing {{{ */
			switch (subframe->type) {
			case IMET4_SFTYPE_PTU:
			case IMET4_SFTYPE_PTUX:
				/* PTUX has the same fields as PTU, plus some extra that we are not
				 * parsing at the moment. To avoid code duplication, "downgrade"
				 * PTUX packets to PTU */
				ptu = (IMET4Subframe_PTU*)subframe;
				pressure = ptu->pressure[0] | ptu->pressure[1] << 8 | ptu->pressure[2] << 16;
				pressure = (pressure << 8) >> 8;

				dst->fields |= DATA_PTU;
				dst->calibrated = 1;
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
				gps = (IMET4Subframe_GPS*)subframe;

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
					gpsx = (IMET4Subframe_GPSX*)subframe;

					hour = gpsx->hour;
					min = gpsx->min;
					sec = gpsx->sec;
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
				self->time = dst->time;

				/* Speed calculation differs between GPS and GPSX */
				dst->fields |= DATA_SPEED;
				switch (subframe->type) {
				case IMET4_SFTYPE_GPS:
					/* Convert to ECEF coordinates to estimate speed vector */
					dt = self->time - self->prev_time;
					lla_to_ecef(&x, &y, &z, dst->lat, dst->lon, dst->alt);
					ecef_to_spd_hdg(&dst->speed, &dst->heading, &dst->climb,
					                dst->lat, dst->lon,
					                (x - self->prev_x)/dt, (y - self->prev_y)/dt, (z - self->prev_z)/dt);

					/* Update last known x/y/z */
					self->prev_x = x;
					self->prev_y = y;
					self->prev_z = z;
					self->prev_time = self->time;
					break;
				case IMET4_SFTYPE_GPSX:
					gpsx = (IMET4Subframe_GPSX*)subframe;

					dst->speed = sqrtf(gpsx->dlon*gpsx->dlon + gpsx->dlat*gpsx->dlat);
					dst->climb = gpsx->climb;
					dst->heading = atan2f(gpsx->dlat, gpsx->dlon) * 180.0 / M_PI;
					if (dst->heading < 0) dst->heading += 360.0;
					break;
				}

				break;

			case IMET4_SFTYPE_XDATA:
				/* TODO */
				break;

			default:
				break;
			}
			/* }}} */

			/* Update pointer to the subframe */
			subframe = (IMET4Subframe*)&self->frame->data[self->frame_offset];
			self->frame_offset += imet4_subframe_len(subframe);
		}

		/* Derive serial number from est. turn-on time */
		if (dst->fields & DATA_SEQ) {
			dst->fields |= DATA_SERIAL;
			sprintf(self->serial, "iMet-%04X", imet4_serial(dst->seq, self->time));
			dst->serial = self->serial;

		}

		self->state = READ_PRE;
		break;
	default:
		break;
	}

	return PARSED;
}

static uint16_t
imet4_serial(int seq, time_t time)
{
	/* Roughly estimate the start-up time */
	time -= seq;

	return crc16_aug_ccitt((uint8_t*)&time, 4);
}
