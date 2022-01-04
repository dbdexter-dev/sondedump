#include <math.h>
#include <stdio.h>
#include "decode/manchester.h"
#include "frame.h"
#include "gps/time.h"
#include "m10.h"

#ifndef NDEBUG
static FILE *debug;
#endif

enum state { READ,
             PARSE_M10_INFO, PARSE_M10_GPS_POS, PARSE_M10_GPS_TIME, PARSE_M10_PTU,
             PARSE_M20_INFO, PARSE_M20_GPS_POS, PARSE_M20_GPS_TIME, PARSE_M20_PTU };

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
	SondeData data = {.type = EMPTY};
	uint8_t *const raw_frame = (uint8_t*)self->frame;
	int inverted;
	int i;
	time_t time;
	float lat, lon, alt;
	float speed, heading, climb;
	float dx, dy, dz;
	M10Frame_9f *data_frame;

	switch (self->state) {
		case READ:
			/* Copy residual bits from the previous frame */
			if (self->offset) {
				self->frame[0] = self->frame[2];
				self->frame[1] = self->frame[3];
			}

			/* Demod until a frame worth of bits is ready */
			if (!gfsk_demod(&self->gfsk, raw_frame, self->offset, M10_FRAME_LEN - self->offset, read)) {
				data.type = SOURCE_END;
				return data;
			}

			/* Find sync marker */
			self->offset = correlate(&self->correlator, &inverted, raw_frame, M10_FRAME_LEN/8);

			/* Align frame being decoded to sync marker */
			if (self->offset) {
				if (!gfsk_demod(&self->gfsk, raw_frame, M10_FRAME_LEN, self->offset, read)) {
					data.type = SOURCE_END;
					return data;
				}
				bitcpy(raw_frame, raw_frame, self->offset, M10_FRAME_LEN);
			}

			/* Correct inverted symbol phase */
			if (inverted) {
				for (i=0; i<M10_FRAME_LEN/8; i++) {
					raw_frame[i] ^= 0xFF;
				}
			}

			/* Manchester decode, then massage bits into shape */
			manchester_decode(raw_frame, raw_frame, M10_FRAME_LEN);
			m10_frame_descramble(self->frame);

			switch (self->frame[0].type) {
				case M10_FTYPE_DATA:
					/* If corrupted, don't decode */
					if (m10_frame_correct(self->frame) < 0) {
						data.type = EMPTY;
						return data;
					}
					/* M10 GPS + PTU data */
					self->state = PARSE_M10_INFO;
					break;
				case M20_FTYPE_DATA:
					break;
				default:
					/* Unknown frame type */
					self->state = READ;
					data.type = EMPTY;
					return data;
			}

#ifndef NDEBUG
			fwrite(&self->frame[0], sizeof(self->frame[0]), 1, debug);
			fflush(debug);
#endif



			break;

		/* M10 frame decoding {{{ */
		case PARSE_M10_INFO:
			data_frame = (M10Frame_9f*)&self->frame[0];

			data.type = INFO;
			m10_frame_9f_serial(self->serial, data_frame);
			data.data.info.sonde_serial = self->serial;

			self->state = PARSE_M10_GPS_TIME;
			break;

		case PARSE_M10_GPS_TIME:
			data_frame = (M10Frame_9f*)&self->frame[0];
			time = m10_frame_9f_time(data_frame);

			data.type = DATETIME;
			data.data.datetime.datetime = time;

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

			data.type = POSITION;
			data.data.pos.lat = lat;
			data.data.pos.lon = lon;
			data.data.pos.alt = alt;
			data.data.pos.speed = speed;
			data.data.pos.heading = heading;
			data.data.pos.climb = climb;

			self->state = PARSE_M10_PTU;

			break;

		case PARSE_M10_PTU:
			/* TODO */
			data.type = FRAME_END;
			self->state = READ;
			break;
		/* }}} */

		default:
			data.type = EMPTY;
			self->state = READ;
			break;
	}

	return data;
}
