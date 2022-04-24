#include <include/imet4.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "decode/ecc/crc.h"
#include "decode/framer.h"
#include "decode/manchester.h"
#include "protocol.h"
#include "frame.h"
#include "subframe.h"

static uint16_t imet4_serial(int seq, time_t time);

struct imet4decoder {
	Framer f;
	IMET4Frame frame[2];
	IMET4Subframe *subframe;
	int state;
	uint32_t time;
	size_t offset, frame_offset;
	char serial[16];
};

enum { READ_PRE, READ, PARSE_SUBFRAME, PARSE_SUBFRAME_PTU_INFO, PARSE_SUBFRAME_GPS_TIME };

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
	size_t subframe_len;
	int32_t pressure;
	struct tm datetime;
	time_t now;
	int seq;
	int hour, min, sec;

	IMET4Subframe_GPS *gps;
	IMET4Subframe_GPSX *gpsx;
	IMET4Subframe_PTU *ptu;
	IMET4Subframe_PTUX *ptux;

	dst->type = EMPTY;

	switch (self->state) {
		case READ_PRE:
			/* Copy residual bits from the previous frame */
			if (self->offset) self->frame[0] = self->frame[1];
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
			fwrite(raw_frame, IMET4_FRAME_LEN/8, 1, debug);
#endif
			self->frame_offset = 0;
			self->state = PARSE_SUBFRAME;
			/* FALLTHROUGH */
		case PARSE_SUBFRAME:
			/* Extract the next subframe */
			self->subframe = (IMET4Subframe*)&self->frame->data[self->frame_offset];
			subframe_len = imet4_subframe_len(self->subframe);

			self->frame_offset += subframe_len;

			/* If the frame is unrecognized or ends out of bounds, we reached the end: read next */
			if (!subframe_len || self->frame_offset >= sizeof(self->frame->data)) {
				dst->type = FRAME_END;
				self->state = READ_PRE;
				break;
			}

			/* Validate the subframe's checksum against the one received. If it
			 * doesn't match, discard it and go to the next */
			if (crc16_aug_ccitt((uint8_t*)self->subframe, subframe_len)) {
				dst->type = EMPTY;
				break;
			}

			/* Subframe parsing {{{ */
			switch (self->subframe->type) {
				case IMET4_SFTYPE_PTU:
					ptu = (IMET4Subframe_PTU*)self->subframe;
					pressure = ptu->pressure[0] | ptu->pressure[1] << 8 | ptu->pressure[2] << 16;
					pressure = (pressure << 8) >> 8;

					dst->type = PTU;

					dst->data.ptu.calibrated = 1;
					dst->data.ptu.calib_percent = 100.0f;
					dst->data.ptu.temp = ptu->temp / 100.0f;
					dst->data.ptu.rh = ptu->rh / 100.0f;
					dst->data.ptu.pressure = pressure / 100.0f;

					self->state = PARSE_SUBFRAME_PTU_INFO;
					break;
				case IMET4_SFTYPE_GPS:
					gps = (IMET4Subframe_GPS*)self->subframe;

					dst->type = POSITION;

					dst->data.pos.lat = gps->lat;
					dst->data.pos.lon = gps->lon;
					dst->data.pos.alt = gps->alt - 5000.0;

					dst->data.pos.speed = 0;
					dst->data.pos.heading = 0;
					dst->data.pos.climb = 0;

					self->state = PARSE_SUBFRAME_GPS_TIME;
					break;
				case IMET4_SFTYPE_PTUX:
					ptux = (IMET4Subframe_PTUX*)self->subframe;
					pressure = ptux->pressure[0] | ptux->pressure[1] << 8 | ptux->pressure[2] << 16;
					pressure = (pressure << 8) >> 8;

					dst->type = PTU;

					dst->data.ptu.calibrated = 1;
					dst->data.ptu.calib_percent = 100.0f;
					dst->data.ptu.temp = ptux->temp / 100.0f;
					dst->data.ptu.rh = ptux->rh / 100.0f;
					dst->data.ptu.pressure = pressure / 100.0f;

					self->state = PARSE_SUBFRAME_PTU_INFO;
					break;
				case IMET4_SFTYPE_GPSX:
					gpsx = (IMET4Subframe_GPSX*)self->subframe;

					dst->type = POSITION;

					dst->data.pos.lat = gpsx->lat;
					dst->data.pos.lon = gpsx->lon;
					dst->data.pos.alt = gpsx->alt + 5000.0;

					dst->data.pos.speed = sqrtf(gpsx->dlat*gpsx->dlat + gpsx->dlon*gpsx->dlon);
					dst->data.pos.heading = atan2f(gpsx->dlat, gpsx->dlon) * 180.0/M_PI;
					if (dst->data.pos.heading < 0) dst->data.pos.heading += 360;
					dst->data.pos.climb = gpsx->climb;

					self->state = PARSE_SUBFRAME_GPS_TIME;
					break;
				case IMET4_SFTYPE_XDATA:
					/* TODO */
					break;

				default:
					dst->type = EMPTY;
					break;
			}
			/* }}} */
			break;
		case PARSE_SUBFRAME_PTU_INFO:
			switch (self->subframe->type) {
				case IMET4_SFTYPE_PTU:
					ptu = (IMET4Subframe_PTU*)self->subframe;
					dst->type = INFO;
					seq = ptu->seq;
					break;
				case IMET4_SFTYPE_PTUX:
					ptux = (IMET4Subframe_PTUX*)self->subframe;
					dst->type = INFO;
					seq = ptux->seq;
					break;
				default:
					dst->type = EMPTY;
					seq = 0;
					break;
			}
			/* Compute serial from start-up time */
			sprintf(self->serial, "iMET4-%04X", imet4_serial(seq, self->time));
			dst->data.info.sonde_serial = self->serial;
			dst->data.info.board_model = "";
			dst->data.info.board_serial = "";

			dst->data.info.seq = seq;

			self->state = PARSE_SUBFRAME;
			break;
		case PARSE_SUBFRAME_GPS_TIME:
			switch (self->subframe->type) {
				case IMET4_SFTYPE_GPS:
					gps = (IMET4Subframe_GPS*)self->subframe;

					dst->type = DATETIME;

					hour = gps->hour;
					min = gps->min;
					sec = gps->sec;
					break;

					break;
				case IMET4_SFTYPE_GPSX:
					gpsx = (IMET4Subframe_GPSX*)self->subframe;

					dst->type = DATETIME;

					hour = gpsx->hour;
					min = gpsx->min;
					sec = gpsx->sec;
					break;
				default:
					dst->type = EMPTY;
					hour = min = sec = 0;
					break;
			}

			now = time(NULL);
			datetime = *gmtime(&now);
			// Handle 0Z crossing
			if (abs(hour - datetime.tm_hour) >= 12) {
				now += (gps->hour < datetime.tm_hour) ? 86400 : -86400;
				datetime = *gmtime(&now);
			}

			datetime.tm_hour = hour;
			datetime.tm_min = min;
			datetime.tm_sec = sec;

			dst->data.datetime.datetime = my_timegm(&datetime);
			self->time = dst->data.datetime.datetime;

			self->state = PARSE_SUBFRAME;
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
