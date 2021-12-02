#include <math.h>
#include <stdio.h>
#include "decode/manchester.h"
#include "frame.h"
#include "gps/time.h"
#include "m10.h"

#ifndef NDEBUG
static FILE *debug;
#endif


void
m10_decoder_init(M10Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, M10_BAUDRATE);
	correlator_init(&d->correlator, M10_SYNCWORD, M10_SYNCLEN);
	d->state = READ;
	d->offset = 0;

#ifndef NDEBUG
	debug = fopen("/tmp/m10frames.data", "wb");
#endif
}

void
m10_decoder_deinit(M10Decoder *d)
{
	gfsk_deinit(&d->gfsk);

#ifndef NDEBUG
	fclose(debug);
#endif
}

SondeData
m10_decode(M10Decoder *self, int (*read)(float *dst))
{
	SondeData data;
	uint8_t *const raw_frame = (uint8_t*)self->frame;
	int inverted;
	int i;
	uint32_t time;
	float lat, lon, alt;
	float speed, heading, climb;
	float dx, dy, dz;
	M10Frame_9f *data_frame;

	switch (self->state) {
		case READ:
			/* Copy residual bits from the previous frame */
			self->frame[0] = self->frame[2];
			self->frame[1] = self->frame[3];

			/* Demod until a frame worth of bits is ready */
			if (!gfsk_demod(&self->gfsk, raw_frame, self->offset, M10_FRAME_LEN*2*8-self->offset, read)) {
				data.type = SOURCE_END;
				return data;
			}

			/* Find sync marker */
			self->offset = correlate(&self->correlator, &inverted, raw_frame, M10_FRAME_LEN*2);

			/* Align frame being decoded to sync marker */
			if (self->offset) {
				if (!gfsk_demod(&self->gfsk, raw_frame + M10_FRAME_LEN*2, 0, self->offset, read)) {
					data.type = SOURCE_END;
					return data;
				}
				bitcpy(raw_frame, raw_frame, self->offset, M10_FRAME_LEN*2*8);
			}

			/* Correct inverted symbol phase */
			if (inverted) {
				for (i=0; i<2*sizeof(*self->frame); i++) {
					raw_frame[i] ^= 0xFF;
				}
			}

			/* Manchester decode, then massage bits into shape */
			manchester_decode(raw_frame, raw_frame, 0, M10_FRAME_LEN*8);
			m10_frame_descramble(self->frame);

			if (self->frame[0].sync_mark[0] != 0x00 ||
				self->frame[0].sync_mark[1] != 0xf0 ||
				self->frame[0].sync_mark[2] != 0x64) {
				data.type = EMPTY;
				return data;
			}

#ifndef NDEBUG
			fwrite(&self->frame[0], sizeof(*self->frame), 1, debug);
#endif
			switch (self->frame[0].type) {
				case M10_FTYPE_DATA:
					/* GPS + PTU data */
					self->state = PARSE_GPS_TIME;
					break;
				default:
					/* Unknown frame type */
					self->state = READ;
					data.type = EMPTY;
					return data;
			}

			__attribute__((fallthrough));
		case PARSE_GPS_TIME:
			data_frame = (M10Frame_9f*)&self->frame[0];
			time = m10_frame_9f_time(data_frame);

			data.type = DATETIME;
			/* FIXME ignoring the entire last byte FIXME */
			data.data.datetime.datetime = time >> 10;
			/* FIXME ignoring the entire last byte FIXME */

			self->state = PARSE_GPS_POS;
			break;

		case PARSE_GPS_POS:
			data_frame = (M10Frame_9f*)&self->frame[0];
			lat = m10_frame_9f_lat(data_frame) * 360.0 / (1UL << 32);
			lon = m10_frame_9f_lon(data_frame) * 360.0 / (1UL << 32);
			alt = m10_frame_9f_alt(data_frame) / 1e3;

			/* FIXME wrong scaling factors FIXME */
			dx = (float)m10_frame_9f_dlat(data_frame) / 200.0;
			dy = (float)m10_frame_9f_dlon(data_frame) / 200.0;
			dz = (float)m10_frame_9f_dalt(data_frame) / 200.0;
			/* FIXME wrong scaling factors FIXME */

			climb = dz;
			speed = sqrtf(dx*dx + dy*dy);
			heading = atan2f(dy, dx) * 180.0 / M_PI;
			if (heading < 0) heading += 360;

			data.type = POSITION;
			data.data.pos.lat = lat;
			data.data.pos.lon = lon;
			data.data.pos.alt = alt;
			data.data.pos.speed = speed;
			data.data.pos.heading = heading;
			data.data.pos.climb = climb;

			self->state = PARSE_PTU;

			break;

		case PARSE_PTU:
			/* TODO */
			data.type = EMPTY;
			self->state = READ;
			break;

		default:
			break;
	}

	return data;
}
