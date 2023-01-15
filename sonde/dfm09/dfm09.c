#include <include/dfm09.h>
#include <stdio.h>
#include <string.h>
#include "bitops.h"
#include "decode/framer.h"
#include "decode/manchester.h"
#include "decode/correlator/correlator.h"
#include "demod/gfsk.h"
#include "frame.h"
#include "log/log.h"
#include "physics.h"
#include "protocol.h"
#include "subframe.h"

typedef struct {
	DFM09Calib calib;
	uint8_t ptu_serial_ch;
	uint64_t raw_serial;
	struct tm time;
} DFM09Data;

static void dfm09_parse_ptu(SondeData *dst, DFM09Data *data, const DFM09Subframe_PTU *subframe);
static void dfm09_parse_gps(SondeData *dst, DFM09Data *data, const DFM09Subframe_GPS *subframe);

struct dfm09decoder {
	Framer f;
	DFM09ECCFrame raw_frame[4];
	DFM09ECCFrame frame[2];
	DFM09Frame parsed_frame;

	SondeData partial_dst;

	DFM09Data data;
};

#ifndef NDEBUG
static FILE *debug;
#endif

__global DFM09Decoder*
dfm09_decoder_init(int samplerate)
{
	DFM09Decoder *d = malloc(sizeof(*d));
	if (!d) return NULL;

	framer_init_gfsk(&d->f, samplerate, DFM09_BAUDRATE, DFM09_FRAME_LEN, DFM09_SYNCWORD, DFM09_SYNC_LEN);
	memset(&d->data, 0, sizeof(d->data));
	d->partial_dst.fields = 0;
#ifndef NDEBUG
	debug = fopen("/tmp/dfm09frames.data", "wb");
#endif

	return d;
}


__global void
dfm09_decoder_deinit(DFM09Decoder *d)
{
	framer_deinit(&d->f);
	free(d);
#ifndef NDEBUG
	if (debug) fclose(debug);
#endif
}


__global ParserStatus
dfm09_decode(DFM09Decoder *self, SondeData *dst, const float *src, size_t len)
{
	DFM09Subframe_PTU *ptu_subframe = &self->parsed_frame.ptu;
	DFM09Subframe_GPS *gps_subframe;
	int i;
	int valid;
	int errcount;

	/* Read a new frame */
	switch (framer_read(&self->f, self->raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}

	/* Rebuild frame from received bits */
	manchester_decode((uint8_t*)self->frame, self->raw_frame, DFM09_FRAME_LEN);
	dfm09_frame_deinterleave(self->frame);

	dst->fields = 0;

	/* Error correct, and exit prematurely if too many errors are found */
	errcount = dfm09_frame_correct(self->frame);
	if (errcount < 0 || errcount > 8) {
		return PARSED;
	}

	/* Remove parity bits */
	dfm09_frame_unpack(&self->parsed_frame, self->frame);

	/* If frame is all zeroes, discard and go to next */
	valid = 0;
	for (i=0; i<(int)sizeof(self->parsed_frame); i++) {
		if (((uint8_t*)&self->parsed_frame)[i] != 0) {
			valid = 1;
			break;
		}
	}
	if (!valid) return PARSED;

#ifndef NDEBUG
	if (debug) fwrite((uint8_t*)&self->parsed_frame, sizeof(self->parsed_frame), 1, debug);
#endif

	/* PTU subframe parsing */
	dfm09_parse_ptu(&self->partial_dst, &self->data, ptu_subframe);

	/* GPS/info subframe parsing */
	for (i=0; i<2; i++) {
		gps_subframe = &self->parsed_frame.gps[i];
		dfm09_parse_gps(&self->partial_dst, &self->data, gps_subframe);
	}

	self->partial_dst.pressure = altitude_to_pressure(self->partial_dst.alt);
	memcpy(dst, &self->partial_dst, sizeof(*dst));
	self->partial_dst.fields = 0;

	return PARSED;
}

/* Static functions {{{ */
static void
dfm09_parse_ptu(SondeData *dst, DFM09Data *data, const DFM09Subframe_PTU *subframe)
{
	uint32_t ch;
	uint16_t serial_shard;
	int serial_idx;
	uint64_t local_serial;

	/* Get channel data as uint24_t */
	ch = bitmerge(subframe->data, 24);
	data->calib.raw[subframe->type] = ch;

	/* Last subframe before serial number is all zeroes */
	if ((ch & 0xFFFF) == 0) {
		data->ptu_serial_ch = subframe->type + 1;
	}

	switch (subframe->type) {
	case DFM_SFTYPE_TEMP:
		/* Temperature */
		dst->temp = dfm09_subframe_temp(subframe, &data->calib);
		dst->fields |= DATA_PTU;
		dst->calib_percent = 100.0;
		break;
	case DFM_SFTYPE_RH:
		/* RH? */
		dst->rh = 0;
		break;
	default:
		if (subframe->type == data->ptu_serial_ch) {
			/* Serial number */

			if (subframe->type == DFM06_SERIAL_TYPE) {
				/* DFM06: direct serial number */
				dst->fields |= DATA_SERIAL;
				sprintf(dst->serial, "D%06X", ch);
			} else {
				/* DFM09: serial number spans multiple subframes */
				serial_idx = 3 - (ch & 0xF);
				serial_shard = (ch >> 4) & 0xFFFF;

				/* Write 16 bit shard into the overall serial number */
				data->raw_serial &= ~((uint64_t)((1 << 16) - 1) << (16*serial_idx));
				data->raw_serial |= (uint64_t)serial_shard << (16*serial_idx);

				/* If potentially complete, convert raw serial to string */
				if ((ch & 0xF) == 0) {
					local_serial = data->raw_serial;

					while (local_serial && !(local_serial & 0xFFFF)) local_serial >>= 16;

					dst->fields |= DATA_SERIAL;
					sprintf(dst->serial, "D%08ld", local_serial);
				}
			}
		}
		break;
	}
}

static void
dfm09_parse_gps(SondeData *dst, DFM09Data *data, const DFM09Subframe_GPS *subframe)
{
	switch (subframe->type) {
	case DFM_SFTYPE_SEQ:
		/* Frame number */
		dst->fields |= DATA_SEQ;
		dst->seq = dfm09_subframe_seq(subframe);
		break;

	case DFM_SFTYPE_TIME:
		/* GPS time */
		data->time.tm_sec = dfm09_subframe_time(subframe);
		break;

	case DFM_SFTYPE_LAT:
		/* Latitude and speed */
		dst->lat = dfm09_subframe_lat(subframe);
		dst->speed = dfm09_subframe_spd(subframe);
		break;

	case DFM_SFTYPE_LON:
		/* Longitude and heading */
		dst->lon = dfm09_subframe_lon(subframe);
		dst->heading = dfm09_subframe_hdg(subframe);
		break;

	case DFM_SFTYPE_ALT:
		/* Altitude and climb rate */
		dst->alt = dfm09_subframe_alt(subframe);
		dst->climb = dfm09_subframe_climb(subframe);
		dst->fields |= DATA_POS | DATA_SPEED;
		break;

	case DFM_SFTYPE_DATE:
		/* GPS date */
		dfm09_subframe_date(&data->time, subframe);

		dst->fields |= DATA_TIME;
		dst->time = my_timegm(&data->time);
		break;

	default:
		break;
	}
}
/* }}} */
