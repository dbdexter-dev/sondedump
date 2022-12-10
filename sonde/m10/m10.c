#include <include/m10.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "demod/gfsk.h"
#include "decode/framer.h"
#include "decode/manchester.h"
#include "frame.h"
#include "subframe.h"
#include "gps/time.h"
#include "physics.h"

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

enum state {
	READ_PRE, READ,
};

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
	float dx, dy, dz;
	M10Frame_9f *data_frame_9f = (M10Frame_9f*)&self->frame[0];
	M20Frame_20 *data_frame_20 = (M20Frame_20*)&self->frame[0];

	switch (self->state) {
	case READ_PRE:
		/* Copy residual bits from the previous frame */
		self->frame[0] = self->frame[2];
		self->frame[1] = self->frame[3];
		self->state = READ;
		/* FALLTHROUGH */
	case READ:
		/* Read a new frame */
		switch (framer_read(&self->f, raw_frame, &self->offset, M10_FRAME_LEN, src, len)) {
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
			 && self->frame[0].sync_mark[2] == 0x88
			 && self->frame[0].type == 0x9f) {
				fwrite(&self->frame[0], sizeof(self->frame[0]), 1, debug);
				fflush(debug);
			}
		}
#endif
		memset(dst, 0, sizeof(*dst));

		switch (self->frame[0].type) {
		case M10_FTYPE_DATA:
			/* M10 frame decoding {{{ */
			/* If corrupted, don't decode */
			if (m10_frame_correct(self->frame) < 0) {
				return PARSED;
			}

			/* Parse serial number */
			dst->fields |= DATA_SERIAL;
			m10_frame_9f_serial(self->serial, data_frame_9f);
			dst->serial = self->serial;

			/* Parse GPS time */
			dst->fields |= DATA_TIME;
			dst->time = m10_frame_9f_time(data_frame_9f);

			/* Parse GPS position */
			dst->fields |= DATA_POS;
			dst->lat = m10_frame_9f_lat(data_frame_9f);
			dst->lon = m10_frame_9f_lon(data_frame_9f);
			dst->alt = m10_frame_9f_alt(data_frame_9f);

			/* Parse GPS speed */
			dx = m10_frame_9f_dlon(data_frame_9f);
			dy = m10_frame_9f_dlat(data_frame_9f);
			dz = m10_frame_9f_dalt(data_frame_9f);

			dst->fields |= DATA_SPEED;
			dst->speed = sqrtf(dx*dx + dy*dy);
			dst->climb = dz;
			dst->heading = atan2f(dy, dx) * 180.0 / M_PI;
			if (dst->heading < 0) dst->heading += 360.0;

			/* Parse PTU data */
			dst->fields |= DATA_PTU;
			dst->calib_percent = 100.0f;
			dst->pressure = altitude_to_pressure(dst->alt);
			dst->temp = m10_frame_9f_temp(data_frame_9f);
			dst->rh = 0; /* TODO */

			/* Prepare for the next read */
			self->state = READ_PRE;
			/* }}} */
			break;
		case M20_FTYPE_DATA:
			/* M20 frame decoding {{{ */
			/* If corrupted, don't decode */
			if (m20_frame_correct(self->frame) < 0) {
				return PARSED;
			}

			/* Parse serial number */
			dst->fields |= DATA_SERIAL;
			m20_frame_20_serial(self->serial, data_frame_20);
			dst->serial = self->serial;

			/* Parse GPS time */
			dst->fields |= DATA_TIME;
			dst->time = m20_frame_20_time(data_frame_20);

			/* Parse GPS position */
			dst->fields |= DATA_POS;
			dst->lat = m20_frame_20_lat(data_frame_20);
			dst->lon = m20_frame_20_lon(data_frame_20);
			dst->alt = m20_frame_20_alt(data_frame_20);

			/* Parse GPS speed */
			dx = m20_frame_20_dlat(data_frame_20);
			dy = m20_frame_20_dlon(data_frame_20);
			dz = m20_frame_20_dalt(data_frame_20);

			dst->fields |= DATA_SPEED;
			dst->climb = dz;
			dst->speed = sqrtf(dx*dx + dy*dy);
			dst->heading = atan2f(dy, dx) * 180.0 / M_PI;
			if (dst->heading < 0) dst->heading += 360;

			/* TODO parse PTU data */

			self->state = READ_PRE;
			/* }}} */
			break;
		default:
			/* Unknown frame type */
			self->state = READ_PRE;
			break;
		}
		break;

	default:
		self->state = READ_PRE;
		break;
	}
	/* }}} */

	return PARSED;
}
