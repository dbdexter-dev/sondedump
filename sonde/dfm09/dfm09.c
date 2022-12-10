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
#include "log/log.h"

struct dfm09decoder {
	Framer f;
	DFM09ECCFrame frame[4];
	DFM09Frame parsed_frame;
	DFM09Calib calib;
	struct tm gps_time;
	int gps_idx, ptu_type_serial;
	SondeData gps_data, ptu_data;
	int state;
	size_t offset;

	char serial[10];
	uint64_t raw_serial;
};

enum { READ_PRE, READ, PARSE_PTU, PARSE_GPS };

#ifndef NDEBUG
static FILE *debug;
#endif

DFM09Decoder*
dfm09_decoder_init(int samplerate)
{
	DFM09Decoder *d = malloc(sizeof(*d));
	if (!d) return NULL;

	framer_init_gfsk(&d->f, samplerate, DFM09_BAUDRATE, DFM09_SYNCWORD, DFM09_SYNC_LEN);
	d->offset = 0;
	d->gps_idx = 0;
	d->raw_serial = 0;
	d->ptu_type_serial = -1;
	d->state = READ_PRE;
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
	framer_deinit(&d->f);
	free(d);
#ifndef NDEBUG
	if (debug) fclose(debug);
#endif
}


ParserStatus
dfm09_decode(DFM09Decoder *self, SondeData *dst, const float *src, size_t len)
{
	DFM09Subframe_GPS *gps_subframe;
	DFM09Subframe_PTU *ptu_subframe;
	int valid;
	int errcount;
	int i;
	uint16_t serial_shard;
	uint64_t local_serial;
	int serial_idx;
	uint8_t *const raw_frame = (uint8_t*)self->frame;

	switch (self->state) {
	case READ_PRE:
		self->frame[0] = self->frame[2];
		self->frame[1] = self->frame[3];
		self->state = READ;
		memset(dst, 0, sizeof(*dst));
		/* FALLTHROUGH */
	case READ:
		/* Read a new frame */
		switch (framer_read(&self->f, raw_frame, &self->offset, DFM09_FRAME_LEN, src, len)) {
		case PROCEED:
			return PROCEED;
		case PARSED:
			break;
		}

		/* Rebuild frame from received bits */
		manchester_decode(raw_frame, raw_frame, DFM09_FRAME_LEN);
		dfm09_frame_deinterleave(self->frame);

#ifndef NDEBUG
		if (debug) {
			fwrite((uint8_t*)self->frame, DFM09_FRAME_LEN/8/2, 1, debug);
			fflush(debug);
		}
#endif

		/* Error correct, and exit prematurely if too many errors are found */
		errcount = dfm09_frame_correct(self->frame);
		if (errcount < 0 || errcount > 8) {
			memset(dst, 0, sizeof(*dst));
			self->state = READ_PRE;
			return PROCEED;
		}

		/* Remove parity bits */
		dfm09_frame_unpack(&self->parsed_frame, self->frame);

		/* If all zeroes, discard */
		valid = 0;
		for (i=0; i<(int)sizeof(self->parsed_frame); i++) {
			if (((uint8_t*)&self->parsed_frame)[i] != 0) {
				valid = 1;
				break;
			}
		}

		if (!valid) {
			memset(dst, 0, sizeof(*dst));
			self->state = READ_PRE;
			return PROCEED;
		}

		/* Prepare for parsing */
		memset(dst, 0, sizeof(*dst));

		/* PTU subframe parsing {{{ */
		ptu_subframe = &self->parsed_frame.ptu;
		self->calib.raw[ptu_subframe->type] = bitmerge(ptu_subframe->data, 24);

		if ((bitmerge(ptu_subframe->data, 24) & 0xFFFF) == 0) {
			self->ptu_type_serial = ptu_subframe->type + 1;
		}

		switch (ptu_subframe->type) {
		case 0x00:
			/* Return previous completed PTU */
			*dst = self->ptu_data;
			dst->fields |= DATA_PTU;
			dst->calibrated = 1;
			dst->calib_percent = 100.0;
			dst->temp = self->ptu_data.temp;
			dst->rh = self->ptu_data.rh;
			dst->pressure = self->ptu_data.pressure;

			/* Temperature */
			self->ptu_data.temp = dfm09_subframe_temp(ptu_subframe, &self->calib);
			break;
		case 0x01:
			/* RH? */
			self->ptu_data.rh = 0;
			break;
		case 0x02:
			/* Pressure? */
			self->ptu_data.pressure = 0;
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
		/* }}} */

		/* GPS/info subframe parsing {{{ */
		for (i=0; i<2; i++) {
			gps_subframe = &self->parsed_frame.gps[i];

			switch (gps_subframe->type) {
			case 0x00:
				/* Frame number */
				dst->fields |= DATA_SEQ | DATA_SERIAL;
				dst->seq = dfm09_subframe_seq(gps_subframe);
				dst->serial = self->serial;
				break;

			case 0x01:
				/* GPS time */
				self->gps_time.tm_sec = dfm09_subframe_time(gps_subframe);
				break;

			case 0x02:
				/* Latitude and speed */
				self->gps_data.lat = dfm09_subframe_lat(gps_subframe);
				self->gps_data.speed = dfm09_subframe_spd(gps_subframe);
				break;

			case 0x03:
				/* Longitude and heading */
				self->gps_data.lon = dfm09_subframe_lon(gps_subframe);
				self->gps_data.heading = dfm09_subframe_hdg(gps_subframe);
				break;

			case 0x04:
				/* Altitude and speed */
				self->gps_data.alt = dfm09_subframe_alt(gps_subframe);
				self->gps_data.climb = dfm09_subframe_climb(gps_subframe);

				dst->fields |= DATA_POS | DATA_SPEED;
				dst->lat = self->gps_data.lat;
				dst->lon = self->gps_data.lon;
				dst->alt = self->gps_data.alt;
				dst->speed = self->gps_data.speed;
				dst->heading = self->gps_data.heading;
				dst->climb = self->gps_data.climb;
				break;

			case 0x08:
				/* GPS date */
				dfm09_subframe_date(&self->gps_time, gps_subframe);

				dst->fields |= DATA_TIME;
				dst->time = my_timegm(&self->gps_time);
				break;

			default:
				break;
			}
		}
		self->state = READ_PRE;
		break;
		/* }}} */

	default:
		self->state = READ;
		break;
	}

	return PARSED;
}
