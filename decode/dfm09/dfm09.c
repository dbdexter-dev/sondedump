#include <stdio.h>
#include "decode/manchester.h"
#include "decode/correlator/correlator.h"
#include "frame.h"
#include "dfm09.h"
#include "subframe.h"

#ifndef NDEBUG
static FILE *debug;
#endif

void
dfm09_decoder_init(DFM09Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, DFM09_BAUDRATE);
	correlator_init(&d->correlator, DFM09_SYNCWORD, DFM09_SYNC_LEN);
	d->gpsIdx = 0;
#ifndef NDEBUG
	debug = fopen("/tmp/dfm09frames.data", "wb");
#endif
}


void
dfm09_decoder_deinit(DFM09Decoder *d)
{
	gfsk_deinit(&d->gfsk);
#ifndef NDEBUG
	fclose(debug);
#endif
}


SondeData
dfm09_decode(DFM09Decoder *self, int (*read)(float *dst))
{
	DFM09Subframe_GPS *gpsSubframe;
	SondeData data = {.type = EMPTY};
	int errcount;
	int offset;
	int inverted;
	uint32_t tmp;

	switch (self->state) {
		case READ:
			/* Read a new frame worth of bits */
			if (!gfsk_demod(&self->gfsk, (uint8_t*)self->frame, 0, DFM09_FRAME_LEN, read)) {
				data.type = SOURCE_END;
				return data;
			}

			offset = correlate(&self->correlator, &inverted, (uint8_t*)self->frame, DFM09_FRAME_LEN);
			if (offset) {
				if (!gfsk_demod(&self->gfsk, (uint8_t*)self->frame + DFM09_FRAME_LEN/8, 0, offset, read)) {
					data.type = SOURCE_END;
					return data;
				}
				bitcpy((uint8_t*)self->frame, (uint8_t*)self->frame, offset, DFM09_FRAME_LEN);
			}

			/* Rebuild frame from received bits */
			dfm09_manchester_decode(self->frame, (uint8_t*)self->frame);
			dfm09_deinterleave(self->frame);

#ifndef NDEBUG
			fwrite((uint8_t*)self->frame, DFM09_FRAME_LEN/8/2, 1, debug);
			fflush(debug);
#endif


			/* Error correct, and exit prematurely if too many errors are found */
			errcount = dfm09_correct(self->frame);
			if (errcount < 0 || errcount > 4) {
				data.type = EMPTY;
				return data;
			}

			/* Remove parity bits */
			dfm09_unpack(&self->parsedFrame, self->frame);
			self->state = PARSE_PTU;
			break;

		case PARSE_PTU:
			/* Parse new PTU data */
			self->calib.raw[self->parsedFrame.ptu.type] = bitmerge(self->parsedFrame.ptu.data, 24);
			switch (self->parsedFrame.ptu.type) {
				case 0x00:
					/* Return previous completed PTU */
					data = self->ptuData;
					data.type = PTU;

					/* Temperature */
					self->ptuData.data.ptu.temp = dfm09_subframe_temp(&self->parsedFrame.ptu, &self->calib);
					break;
				case 0x01:
					self->ptuData.data.ptu.rh = 0;
					break;
				case 0x02:
					self->ptuData.data.ptu.pressure = 0;
					break;
				default:
					break;
			}

			self->gpsIdx = 0;
			self->state = PARSE_GPS;
			break;

		case PARSE_GPS:
			/* Parse GPS subframe */
			gpsSubframe = &self->parsedFrame.gps[self->gpsIdx++];
			if (self->gpsIdx > 2) {
				self->state = READ;
				break;
			}

			switch (gpsSubframe->type) {
				case 0x00:
					/* Frame number */
					data.type = INFO;
					data.data.info.seq = bitmerge(gpsSubframe->data, 32);
					data.data.info.sonde_serial = "";
					data.data.info.board_model = "";
					data.data.info.board_serial = "";
					break;

				case 0x01:
					/* GPS time */
					self->gpsTime.tm_sec = bitmerge(gpsSubframe->data + 4, 16) / 1000;
					break;

				case 0x02:
					/* Latitude and speed */
					self->gpsData.data.pos.lat = (int32_t)bitmerge(gpsSubframe->data, 32) / 1e7;
					self->gpsData.data.pos.speed = bitmerge(gpsSubframe->data + 4, 16) / 1e2;
					break;

				case 0x03:
					/* Longitude and heading */
					self->gpsData.data.pos.lon = (int32_t)bitmerge(gpsSubframe->data, 32) / 1e7;
					self->gpsData.data.pos.heading = bitmerge(gpsSubframe->data + 4, 16) / 1e2;
					break;

				case 0x04:
					/* Altitude and speed */
					self->gpsData.data.pos.alt = (int32_t)bitmerge(gpsSubframe->data, 32) / 1e7;
					self->gpsData.data.pos.climb = (int16_t)bitmerge(gpsSubframe->data + 4, 16) / 1e2;
					data = self->gpsData;
					data.type = POSITION;
					break;

				case 0x08:
					/* GPS date */
					tmp = bitmerge(gpsSubframe->data, 32);

					self->gpsTime.tm_year = (tmp >> (32 - 12) & 0xFFF) - 1900;
					self->gpsTime.tm_mon = tmp >> (32 - 16) & 0xF;
					self->gpsTime.tm_mday = tmp >> (32 - 21) & 0x1F;
					self->gpsTime.tm_hour = tmp >> (32 - 26) & 0x1F;
					self->gpsTime.tm_min = tmp & 0x3F;

					data.type = DATETIME;
					data.data.datetime.datetime = timegm(&self->gpsTime);
					break;

				default:
					break;
			}
			break;

		default:
			self->state = READ;
			break;
	}

	return data;
}
