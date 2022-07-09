#include <include/rs41.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitops.h"
#include "decode/framer.h"
#include "decode/ecc/crc.h"
#include "decode/xdata.h"
#include "frame.h"
#include "gps/ecef.h"
#include "gps/time.h"
#include "log/log.h"
#include "physics.h"
#include "subframe.h"

typedef struct {
	int initialized;
	RS41Calibration data;
	uint8_t bitmask[sizeof(RS41Calibration)/8/RS41_CALIB_FRAGSIZE+1];
} RS41Metadata;


struct rs41decoder {
	Framer f;
	RSDecoder rs;
	RS41Frame frame[2];
	int state;
	size_t offset, frame_offset;
	int calibrated;
	float calib_percent;
	RS41Metadata metadata;
	float pressure;
	char serial[9];
};

static float rs41_update_metadata(RS41Metadata *m, RS41Subframe_Info *s);
static int rs41_metadata_ptu_calibrated(RS41Metadata *m);

#ifndef NDEBUG
static FILE *debug;
#endif

/* Sane default calibration data, taken from a live radiosonde {{{ */
static const uint8_t _default_calib_data[sizeof(RS41Calibration)] = {
	0xec, 0x5c, 0x80, 0x57, 0x03, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x53, 0x33, 0x32,
	0x32, 0x30, 0x36, 0x35, 0x30, 0xf7, 0x4e, 0x00, 0x00, 0x58, 0x02, 0x12, 0x05, 0xb4, 0x3c, 0xa4,
	0x06, 0x14, 0x87, 0x32, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x03, 0x1e, 0x23,
	0xe8, 0x03, 0x01, 0x04, 0x00, 0x07, 0x00, 0xbf, 0x02, 0x91, 0xb3, 0x00, 0x06, 0x00, 0x80, 0x3b,
	0x44, 0x00, 0x80, 0x89, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x42, 0x2a, 0xe9, 0x73,
	0xc3, 0x5f, 0x28, 0x40, 0x3e, 0xbb, 0x92, 0x09, 0x37, 0xdd, 0xd6, 0xa0, 0x3f, 0xc5, 0x52, 0xd6,
	0xbd, 0x54, 0xe4, 0xb5, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x99, 0x30, 0x42, 0x6f, 0xd9, 0xa1, 0x40, 0xe1, 0x79, 0x29,
	0xbb, 0x52, 0x98, 0x0f, 0xc0, 0x5f, 0xc4, 0x1e, 0x41, 0xc3, 0x9f, 0x67, 0xc0, 0xe9, 0x6b, 0x59,
	0x42, 0x33, 0x9a, 0xba, 0xc2, 0x8e, 0xd2, 0x4e, 0x42, 0xc3, 0x7b, 0x1b, 0x42, 0xf8, 0x6f, 0x51,
	0x43, 0xf0, 0x37, 0xbd, 0xc3, 0xa8, 0xc5, 0x12, 0x41, 0x93, 0x3d, 0x9c, 0x41, 0xeb, 0x41, 0x16,
	0x43, 0x14, 0xe8, 0x16, 0xc3, 0x45, 0x28, 0x8c, 0xc3, 0x09, 0x4b, 0x36, 0x43, 0x4f, 0xf6, 0x4a,
	0x45, 0x6f, 0x3a, 0x7f, 0x45, 0x86, 0x91, 0x69, 0xc3, 0xf1, 0xaf, 0xac, 0x43, 0x8d, 0x37, 0x48,
	0x43, 0x7b, 0x1f, 0xc2, 0xc3, 0x87, 0x1a, 0x62, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x54, 0xd7, 0x61,
	0x43, 0xf4, 0x0c, 0x69, 0xc3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x20, 0xba, 0xc2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2a, 0xe9, 0x73, 0xc3, 0x5f, 0x28, 0x40, 0x3e, 0xbb, 0x92, 0x09,
	0x37, 0x80, 0xda, 0xa5, 0x3f, 0xa6, 0x1d, 0xc0, 0xbc, 0x82, 0x9e, 0xb3, 0x3b, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xff, 0xff, 0xff, 0xc6, 0x00, 0x41, 0x69, 0x30, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xcd, 0xcc, 0xcc, 0x3d, 0xbd, 0xff, 0x4b, 0xbf, 0x47, 0x49, 0x9e, 0xbd, 0x66, 0x36, 0xb1, 0x33,
	0x5b, 0x39, 0x8b, 0xb7, 0x1b, 0x8a, 0xf1, 0x39, 0x00, 0xe0, 0xaa, 0x44, 0xf0, 0x85, 0x49, 0x3c,
	0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x90, 0x40, 0x00, 0x00, 0xa0, 0x3f, 0x00, 0x00, 0x00, 0x00,
	0x33, 0x33, 0x33, 0x3f, 0x68, 0x91, 0x2d, 0x3f, 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xe6, 0x96, 0x7e, 0x3f, 0x97, 0x82, 0x9b, 0xb8, 0xaa, 0x39, 0x23, 0x30,
	0xe4, 0x16, 0xcd, 0x29, 0xb5, 0x26, 0x5a, 0xa2, 0xfd, 0xeb, 0x02, 0x1a, 0xec, 0x51, 0x38, 0x3e,
	0x33, 0x33, 0x33, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xf6, 0x7f, 0x74, 0x40, 0x3b, 0x36, 0x82, 0xbf, 0xe5, 0x2f, 0x98, 0x3d, 0x00, 0x01, 0x00, 0x01,
	0xac, 0xac, 0xba, 0xbe, 0x0c, 0xe6, 0xab, 0x3e, 0x00, 0x00, 0x00, 0x40, 0x08, 0x39, 0xad, 0x41,
	0x89, 0x04, 0xaf, 0x41, 0x00, 0x00, 0x40, 0x40, 0xff, 0xff, 0xff, 0xc6, 0xff, 0xff, 0xff, 0xc6,
	0xff, 0xff, 0xff, 0xc6, 0xff, 0xff, 0xff, 0xc6, 0x52, 0x53, 0x34, 0x31, 0x2d, 0x53, 0x47, 0x00,
	0x00, 0x00, 0x52, 0x53, 0x4d, 0x34, 0x31, 0x32, 0x00, 0x00, 0x00, 0x00, 0x53, 0x33, 0x31, 0x31,
	0x30, 0x33, 0x31, 0x34, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00,
	0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x81, 0x23, 0x00,
	0x00, 0x1a, 0x02, 0x00, 0x02, 0x7b, 0xe5, 0xb5, 0x3f, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd5, 0xca, 0xa4, 0x3d, 0x5d, 0xa3, 0x65, 0x39, 0x7f, 0x87,
	0x22, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0xfe, 0xb7, 0xbc, 0xc8, 0x96,
	0xe5, 0x3e, 0x31, 0x99, 0x1a, 0xbf, 0x12, 0xda, 0xda, 0x3e, 0xb6, 0x84, 0x68, 0xc1, 0x67, 0x55,
	0x57, 0x42, 0xd6, 0xc5, 0xaa, 0xc1, 0x84, 0x9e, 0xc7, 0xc1, 0xfd, 0xbc, 0x3e, 0x41, 0x1e, 0x16,
	0x4c, 0xc2, 0x7c, 0xb8, 0x8b, 0x41, 0xbb, 0x32, 0xf4, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x14, 0x00,
	0xc8, 0x00, 0x46, 0x00, 0x3c, 0x00, 0x05, 0x00, 0x3c, 0x00, 0x18, 0x01, 0x9e, 0x62, 0xd5, 0xb8,
	0x6c, 0x9c, 0x07, 0xb1, 0x00, 0x3c, 0x88, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xf3, 0x6a, 0xc0, 0xf1, 0x5b, 0x02, 0x07, 0x00, 0x00, 0x05, 0x6d, 0x01, 0x1b, 0x94,
};
/* }}} */

enum { READ_PRE, READ, PARSE_SUBFRAME };

RS41Decoder*
rs41_decoder_init(int samplerate)
{
	RS41Decoder *d = malloc(sizeof(*d));
	framer_init_gfsk(&d->f, samplerate, RS41_BAUDRATE, RS41_SYNCWORD, RS41_SYNC_LEN);
	rs_init(&d->rs, RS41_REEDSOLOMON_N, RS41_REEDSOLOMON_K, RS41_REEDSOLOMON_POLY,
			RS41_REEDSOLOMON_FIRST_ROOT, RS41_REEDSOLOMON_ROOT_SKIP);

	d->state = READ;
	d->metadata.initialized = 0;
	d->pressure = 0;
	d->offset = 0;

	/* Initialize calibration data struct and metadata */
	memcpy(&d->metadata.data, _default_calib_data, sizeof(d->metadata.data));
	memset(&d->metadata.bitmask, 0x00, sizeof(d->metadata.bitmask));
	d->metadata.bitmask[sizeof(d->metadata.bitmask)-1] |= ((1 << (8 - RS41_CALIB_FRAGCOUNT%8)) - 1);
	d->metadata.data.burstkill_timer = 0xFFFF;

#ifndef NDEBUG
	debug = fopen("/tmp/rs41frames.data", "wb");
#endif

	return d;
}

void
rs41_decoder_deinit(RS41Decoder *d)
{
	framer_deinit(&d->f);
	free(d);
#ifndef NDEBUG
	if (debug) fclose(debug);
#endif
}

ParserStatus
rs41_decode(RS41Decoder *self, SondeData *dst, const float *src, size_t len)
{
	RS41Subframe *subframe;
	size_t frame_data_len;
	int burstkill_timer;
	float x, y, z, dx, dy, dz;
	uint8_t *const raw_frame = (uint8_t*)self->frame;

	RS41Subframe_Info *status;
	RS41Subframe_PTU *ptu;
	RS41Subframe_GPSInfo *gpsinfo;
	RS41Subframe_GPSPos *gpspos;
	RS41Subframe_XDATA *xdata;

	dst->type = EMPTY;

	switch (self->state) {
		case READ_PRE:
			/* Copy residual bits from the previous frame */
			if (self->offset && !(self->offset % sizeof(self->frame[0]))) log_debug("Bug triggered");
			if (self->offset) self->frame[0] = self->frame[1];
			self->state = READ;
			/* FALLTHROUGH */
		case READ:
			/* Read a new frame */
			switch (framer_read(&self->f, raw_frame, &self->offset, RS41_FRAME_LEN, src, len)) {
				case PROCEED:
					return PROCEED;
				case PARSED:
					break;
			}

			/* Descramble and error correct */
			rs41_frame_descramble(self->frame);
			rs41_frame_correct(self->frame, &self->rs);

#ifndef NDEBUG
			if (debug) {
				/* Output the frame to file */
				fwrite(self->frame, RS41_FRAME_LEN/8, 1, debug);
			}
#endif

			/* Prepare to parse subframes */
			self->frame_offset = 0;
			self->pressure = 0;
			self->state = PARSE_SUBFRAME;
			/* FALLTHROUGH */

		case PARSE_SUBFRAME:
			/* Parse expected data length from extended flag */
			frame_data_len = RS41_DATA_LEN + (rs41_frame_is_extended(self->frame) ? RS41_XDATA_LEN : 0);

			/* Update pointer to the subframe */
			subframe = (RS41Subframe*)&self->frame->data[self->frame_offset];
			self->frame_offset += subframe->len + 4;

			/* If the frame ends out of bounds, we reached the end: read next */
			if (self->frame_offset >= frame_data_len) {
				dst->type = FRAME_END;
				self->state = READ_PRE;
				break;
			}

			/* Validate the subframe's checksum against the one received.
			 * If it doesn't match, discard it */
			if (crc16_ccitt_false(subframe->data, subframe->len) != *(uint16_t*)&subframe->data[subframe->len]) {
				log_debug("Incorrect CRC, skipping");
				dst->type = EMPTY;
				break;
			}

			/* Subframe parsing {{{ */
			switch (subframe->type) {
				case RS41_SFTYPE_EMPTY:
					/* Padding */
					dst->type = EMPTY;
					break;
				case RS41_SFTYPE_INFO:
					/* Frame sequence number, serial no., board info, calibration data */
					status = (RS41Subframe_Info*)subframe;
					self->calib_percent = rs41_update_metadata(&self->metadata, (RS41Subframe_Info*)subframe);
					self->calibrated = rs41_metadata_ptu_calibrated(&self->metadata);

					dst->type = INFO;

					strncpy(self->serial, status->serial, 8);
					self->serial[8] = 0;
					dst->data.info.seq = status->frame_seq;
					dst->data.info.sonde_serial = self->serial;
					burstkill_timer = self->metadata.data.burstkill_timer;
					dst->data.info.burstkill_status = (burstkill_timer == 0xFFFF ? -1 : burstkill_timer);
					break;
				case RS41_SFTYPE_PTU:
					/* Temperature, humidity, pressure */
					ptu = (RS41Subframe_PTU*)subframe;

					dst->type = PTU;

					dst->data.ptu.temp = rs41_subframe_temp(ptu, &self->metadata.data);
					dst->data.ptu.rh = rs41_subframe_humidity(ptu, &self->metadata.data);
					dst->data.ptu.pressure = rs41_subframe_pressure(ptu, &self->metadata.data);
					dst->data.ptu.calibrated = self->calibrated;
					dst->data.ptu.calib_percent = self->calib_percent;
					self->pressure = dst->data.ptu.pressure;
					break;
				case RS41_SFTYPE_GPSPOS:
					/* GPS position */
					gpspos = (RS41Subframe_GPSPos*)subframe;

					x = rs41_subframe_x(gpspos);
					y = rs41_subframe_y(gpspos);
					z = rs41_subframe_z(gpspos);
					dx = rs41_subframe_dx(gpspos);
					dy = rs41_subframe_dy(gpspos);
					dz = rs41_subframe_dz(gpspos);

					dst->type = POSITION;

					ecef_to_lla(&dst->data.pos.lat, &dst->data.pos.lon, &dst->data.pos.alt, x, y, z);
					ecef_to_spd_hdg(&dst->data.pos.speed, &dst->data.pos.heading, &dst->data.pos.climb,
							dst->data.pos.lat, dst->data.pos.lon, dx, dy, dz);

					/* If pressure is not provided, estimate it from altitude
					 * so that ozone ppb calculation can stil happen */
					if (!(self->pressure > 0)) self->pressure = altitude_to_pressure(dst->data.pos.alt);

					break;
				case RS41_SFTYPE_GPSINFO:
					/* GPS date/time and RSSI */
					gpsinfo = (RS41Subframe_GPSInfo*)subframe;

					dst->type = DATETIME;

					dst->data.datetime.datetime = gps_time_to_utc(gpsinfo->week, gpsinfo->ms);
					break;
				case RS41_SFTYPE_XDATA:
					/* XDATA */
					xdata = (RS41Subframe_XDATA*)subframe;
					dst->type = XDATA;
					dst->data.xdata.data = xdata_decode(self->pressure, xdata->ascii_data, xdata->len-1);
					break;
				default:
					/* Unknown */
					dst->type = UNKNOWN;
					break;
			}
			/* }}} */


			break;
		default:
			dst->type = UNKNOWN;
			break;
	}

	return PARSED;
}

/* Static functions {{{ */
static float
rs41_update_metadata(RS41Metadata *m, RS41Subframe_Info *s)
{
	size_t frag_offset;
	int num_received;

	/* Copy the fragment and update the bitmap of the fragments left */
	frag_offset = s->frag_seq * LEN(s->frag_data);
	memcpy((uint8_t*)&m->data + frag_offset, s->frag_data, LEN(s->frag_data));
	m->bitmask[s->frag_seq/8] |= (1 << (7 - s->frag_seq%8));

	/* Check if we have all the sub-segments populated */
	num_received = count_ones(m->bitmask, sizeof(m->bitmask));

	return (num_received * 100.0) / (8 * sizeof(m->bitmask));
}

static int
rs41_metadata_ptu_calibrated(RS41Metadata *m)
{
	const uint8_t cmp[] = {0x1F, 0xFF, 0xF8, 0x00, 0x0E};
	size_t i;

	for (i=0; i<LEN(cmp); i++) {
		if ((m->bitmask[i] & cmp[i]) != cmp[i]) return 0;
	}

	return 1;
}
/* }}} */
