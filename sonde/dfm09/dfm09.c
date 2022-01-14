#include <include/dfm09.h>
#include <stdio.h>
#include <string.h>
#include "decode/framer.h"
#include "decode/manchester.h"
#include "decode/correlator/correlator.h"
#include "demod/gfsk.h"
#include "frame.h"
#include "protocol.h"
#include "subframe.h"
#include "bitops.h"

struct dfm09decoder {
	GFSKDemod gfsk;
	Correlator correlator;
	DFM09ECCFrame frame[4];
	DFM09Frame parsed_frame;
	DFM09Calib calib;
	struct tm gps_time;
	int gps_idx, ptu_type_serial;
	SondeData gps_data, ptu_data;
	int state;

	char serial[10];
	uint64_t raw_serial;
};

enum { READ, PARSE_PTU, PARSE_GPS };

#ifndef NDEBUG
static FILE *debug;
#endif

DFM09Decoder*
dfm09_decoder_init(int samplerate)
{
	DFM09Decoder *d = malloc(sizeof(*d));
	if (!d) return NULL;

	gfsk_init(&d->gfsk, samplerate, DFM09_BAUDRATE);
	correlator_init(&d->correlator, DFM09_SYNCWORD, DFM09_SYNC_LEN);
	d->gps_idx = 0;
	d->raw_serial = 0;
	d->ptu_type_serial = -1;
	memset(&d->ptu_data, 0, sizeof(d->ptu_data));
	memset(&d->gps_data, 0, sizeof(d->gps_data));
	memset(&d->serial, 0, sizeof(d->serial));
#ifndef NDEBUG
	debug = fopen("/tmp/dfm09frames.data", "wb");
#endif

	return d;
}


void
dfm09_decoder_deinit(DFM09Decoder *d)
{
	gfsk_deinit(&d->gfsk);
	free(d);
#ifndef NDEBUG
	fclose(debug);
#endif
}


SondeData
dfm09_decode(DFM09Decoder *self, int (*read)(float *dst, size_t len))
{
	DFM09Subframe_GPS *gpsSubframe;
	DFM09Subframe_PTU *ptu_subframe;
	SondeData data = {.type = EMPTY};
	int errcount;
	int i;
	uint16_t serial_shard;
	uint64_t local_serial;
	int serial_idx;
	uint8_t *const raw_frame = (uint8_t*)self->frame;

	switch (self->state) {
		case READ:
			/* Read a new frame */
			if (read_frame_gfsk(&self->gfsk, &self->correlator, raw_frame, read, DFM09_FRAME_LEN, 0) < 0) {
				data.type = SOURCE_END;
				return data;
			}

			/* Rebuild frame from received bits */
			manchester_decode(raw_frame, raw_frame, DFM09_FRAME_LEN);
			dfm09_frame_deinterleave(self->frame);

#ifndef NDEBUG
			fwrite((uint8_t*)self->frame, DFM09_FRAME_LEN/8/2, 1, debug);
			fflush(debug);
#endif

			/* Error correct, and exit prematurely if too many errors are found */
			errcount = dfm09_frame_correct(self->frame);
			if (errcount < 0 || errcount > 8) {
				data.type = EMPTY;
				return data;
			}

			/* Remove parity bits */
			dfm09_frame_unpack(&self->parsed_frame, self->frame);

			/* If all zeroes, discard */
			for (i=0; i<(int)sizeof(self->parsed_frame); i++) {
				if (((uint8_t*)&self->parsed_frame)[i] != 0) {
					self->state = PARSE_PTU;
					break;
				}
			}
			break;

		case PARSE_PTU:
			/* PTU subframe parsing {{{ */
			ptu_subframe = &self->parsed_frame.ptu;
			self->calib.raw[ptu_subframe->type] = bitmerge(ptu_subframe->data, 24);

			if ((bitmerge(ptu_subframe->data, 24) & 0xFFFF) == 0) {
				self->ptu_type_serial = ptu_subframe->type + 1;
			}

			switch (ptu_subframe->type) {
				case 0x00:
					/* Return previous completed PTU */
					data = self->ptu_data;
					data.type = PTU;
					data.data.ptu.calibrated = 1;
					data.data.ptu.calib_percent = 100.0f;

					/* Temperature */
					self->ptu_data.data.ptu.temp = dfm09_subframe_temp(ptu_subframe, &self->calib);
					break;
				case 0x01:
					/* RH? */
					self->ptu_data.data.ptu.rh = 0;
					break;
				case 0x02:
					/* Pressure? */
					self->ptu_data.data.ptu.pressure = 0;
					break;
				default:
					if (ptu_subframe->type == self->ptu_type_serial) {
						/* Serial number */
						if (ptu_subframe->type == DFM06_SERIAL_TYPE) {
							/* DFM06: direct serial number */
							sprintf(self->serial, "D%06lX", bitmerge(ptu_subframe->data, 24));
						} else {
							/* DFM09: serial number spans multiple subframes */
							serial_idx = 3 - (bitmerge(ptu_subframe->data, 24) & 0xF);
							serial_shard = (bitmerge(ptu_subframe->data, 24) >> 4) & 0xFFFF;

							/* Write 16 bit shard into the overall serial number */
							self->raw_serial &= ~((uint64_t)((1 << 16) - 1) << (16*serial_idx));
							self->raw_serial |= (uint64_t)serial_shard << (16*serial_idx);

							/* If potentially complete, convert raw serial to string */
							if ((bitmerge(ptu_subframe->data, 24) & 0xF) == 0) {
								local_serial = self->raw_serial;
								while (!(local_serial & 0xFFFF)) local_serial >>= 16;
								sprintf(self->serial, "D%08ld", local_serial);
							}
						}
					}
					break;
			}

			self->gps_idx = 0;
			self->state = PARSE_GPS;
			break;
			/* }}} */

		case PARSE_GPS:
			/* GPS/info subframe parsing {{{ */
			gpsSubframe = &self->parsed_frame.gps[self->gps_idx++];
			if (self->gps_idx > 2) {
				data.type = FRAME_END;
				self->state = READ;
				break;
			}

			switch (gpsSubframe->type) {
				case 0x00:
					/* Frame number */
					data.type = INFO;
					data.data.info.seq = dfm09_subframe_seq(gpsSubframe);
					data.data.info.sonde_serial = self->serial;
					data.data.info.board_model = "";
					data.data.info.board_serial = "";
					break;

				case 0x01:
					/* GPS time */
					self->gps_time.tm_sec = dfm09_subframe_time(gpsSubframe);
					break;

				case 0x02:
					/* Latitude and speed */
					self->gps_data.data.pos.lat = dfm09_subframe_lat(gpsSubframe);
					self->gps_data.data.pos.speed = dfm09_subframe_spd(gpsSubframe);
					break;

				case 0x03:
					/* Longitude and heading */
					self->gps_data.data.pos.lon = dfm09_subframe_lon(gpsSubframe);
					self->gps_data.data.pos.heading = dfm09_subframe_hdg(gpsSubframe);
					break;

				case 0x04:
					/* Altitude and speed */
					self->gps_data.data.pos.alt = dfm09_subframe_alt(gpsSubframe);
					self->gps_data.data.pos.climb = dfm09_subframe_climb(gpsSubframe);
					data = self->gps_data;
					data.type = POSITION;
					break;

				case 0x08:
					/* GPS date */
					dfm09_subframe_date(&self->gps_time, gpsSubframe);

					data.type = DATETIME;
					data.data.datetime.datetime = my_timegm(&self->gps_time);
					break;

				default:
					break;
			}
			break;
			/* }}} */

		default:
			self->state = READ;
			break;
	}

	return data;
}
