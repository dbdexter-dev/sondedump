#include <include/m10.h>
#include <math.h>
#include <stdio.h>
#include "demod/gfsk.h"
#include "decode/framer.h"
#include "decode/manchester.h"
#include "frame.h"
#include "subframe.h"
#include "gps/time.h"

struct m10decoder {
	Framer f;
	M10Frame frame[4];
	size_t offset;
	int state;
	char serial[16];
};

#ifndef NDEBUG
static FILE *debug;
#endif

enum state { READ_PRE, READ,
             PARSE_M10_INFO, PARSE_M10_GPS_POS, PARSE_M10_GPS_TIME, PARSE_M10_PTU,
             PARSE_M20_INFO, PARSE_M20_GPS_POS, PARSE_M20_GPS_TIME, PARSE_M20_PTU };

M10Decoder*
m10_decoder_init(int samplerate)
{
	M10Decoder *d = malloc(sizeof(*d));

	framer_init_gfsk(&d->f, samplerate, M10_BAUDRATE, M10_SYNCWORD, M10_SYNC_LEN);
	d->state = READ;
	d->offset = 0;

#ifndef NDEBUG
	debug = fopen("/tmp/m10frames.data", "wb");
#endif

	return d;
}

void
m10_decoder_deinit(M10Decoder *d)
{
	framer_deinit(&d->f);
	free(d);
#ifndef NDEBUG
	if (debug) fclose(debug);
#endif
}

ParserStatus
m10_decode(M10Decoder *self, SondeData *dst, const float *src, size_t len)
{
	uint8_t *const raw_frame = (uint8_t*)self->frame;
	time_t time;
	float lat, lon, alt;
	float speed, heading, climb;
	float dx, dy, dz;
	M10Frame_9f *data_frame;

	switch (self->state) {
		case READ_PRE:
			/* Copy residual bits from the previous frame */
			if (self->offset) {
				self->frame[0] = self->frame[2];
				self->frame[1] = self->frame[3];
			}
			self->state = READ;
			/* FALLTHROUGH */
		case READ:
			/* Read a new frame */
			switch (read_frame_gfsk(&self->f, raw_frame, &self->offset, M10_FRAME_LEN, src, len)) {
				case PROCEED:
					return PROCEED;
				case PARSED:
					break;
			}

			/* Manchester decode, then massage bits into shape */
			manchester_decode(raw_frame, raw_frame, M10_FRAME_LEN);
			m10_frame_descramble(self->frame);

#ifndef NDEBUG
			if (debug) {
				if (self->frame[0].sync_mark[0] == 0x00
				 && self->frame[0].sync_mark[1] == 0x00
				 && self->frame[0].sync_mark[2] == 0x88) {
					fwrite(&self->frame[0], sizeof(self->frame[0]), 1, debug);
					fflush(debug);
				}
			}
#endif
			switch (self->frame[0].type) {
				case M10_FTYPE_DATA:
					/* If corrupted, don't decode */
					if (m10_frame_correct(self->frame) < 0) {
						dst->type = EMPTY;
						return PARSED;
					}
					/* M10 GPS + PTU data */
					self->state = PARSE_M10_INFO;
					break;
				case M20_FTYPE_DATA:
					break;
				default:
					/* Unknown frame type */
					self->state = READ_PRE;
					dst->type = EMPTY;
					return PARSED;
			}

			break;

		/* M10 frame decoding {{{ */
		case PARSE_M10_INFO:
			data_frame = (M10Frame_9f*)&self->frame[0];

			dst->type = INFO;
			m10_frame_9f_serial(self->serial, data_frame);
			dst->data.info.sonde_serial = self->serial;
			dst->data.info.seq = 0;

			self->state = PARSE_M10_GPS_TIME;
			break;

		case PARSE_M10_GPS_TIME:
			data_frame = (M10Frame_9f*)&self->frame[0];
			time = m10_frame_9f_time(data_frame);

			dst->type = DATETIME;
			dst->data.datetime.datetime = time;

			self->state = PARSE_M10_GPS_POS;
			break;

		case PARSE_M10_GPS_POS:
			data_frame = (M10Frame_9f*)&self->frame[0];
			lat = m10_frame_9f_lat(data_frame);
			lon = m10_frame_9f_lon(data_frame);
			alt = m10_frame_9f_alt(data_frame);

			dx = m10_frame_9f_dlat(data_frame);
			dy = m10_frame_9f_dlon(data_frame);
			dz = m10_frame_9f_dalt(data_frame);

			climb = dz;
			speed = sqrtf(dx*dx + dy*dy);
			heading = atan2f(dy, dx) * 180.0 / M_PI;
			if (heading < 0) heading += 360;

			dst->type = POSITION;
			dst->data.pos.lat = lat;
			dst->data.pos.lon = lon;
			dst->data.pos.alt = alt;
			dst->data.pos.speed = speed;
			dst->data.pos.heading = heading;
			dst->data.pos.climb = climb;

			self->state = PARSE_M10_PTU;

			break;

		case PARSE_M10_PTU:
			/* TODO */
			dst->type = FRAME_END;
			self->state = READ_PRE;
			break;
		/* }}} */

		default:
			dst->type = EMPTY;
			self->state = READ_PRE;
			break;
	}

	return PARSED;
}
