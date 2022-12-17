#include <include/rs41.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitops.h"
#include "decode/framer.h"
#include "decode/ecc/crc.h"
#include "frame.h"
#include "gps/ecef.h"
#include "gps/time.h"
#include "log/log.h"
#include "physics.h"
#include "subframe.h"
#include "xdata/xdata.h"

typedef struct {
	int initialized;
	RS41Calibration data;
	uint8_t bitmask[sizeof(RS41Calibration)/8/RS41_CALIB_FRAGSIZE+1];
} RS41Metadata;


struct rs41decoder {
	Framer f;
	RSDecoder rs;
	RS41Frame raw_frame[2];
	RS41Frame frame;
	size_t offset;
	RS41Metadata metadata;
};

static void rs41_parse_subframe(SondeData *dst, RS41Subframe *subframe, RS41Metadata *metadata);
static void rs41_update_metadata(RS41Metadata *m, RS41Subframe_Info *s);
static float rs41_get_calib_percent(RS41Metadata *m);
static int rs41_metadata_ptu_calibrated(RS41Metadata *m);
static void rs41_xdata_decode(SondeXdata *dst, float curPressure, const char *asciiData, int len);

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

RS41Decoder*
rs41_decoder_init(int samplerate)
{
	RS41Decoder *d = malloc(sizeof(*d));
	framer_init_gfsk(&d->f, samplerate, RS41_BAUDRATE, RS41_SYNCWORD, RS41_SYNC_LEN);
	rs_init(&d->rs, RS41_REEDSOLOMON_N, RS41_REEDSOLOMON_K, RS41_REEDSOLOMON_POLY,
			RS41_REEDSOLOMON_FIRST_ROOT, RS41_REEDSOLOMON_ROOT_SKIP);

	d->metadata.initialized = 0;
	d->offset = 0;

	/* Initialize calibration data struct and metadata */
	memcpy(&d->metadata.data, _default_calib_data, sizeof(d->metadata.data));
	memset(&d->metadata.bitmask, 0x00, sizeof(d->metadata.bitmask));

#ifndef NDEBUG
	debug = fopen("/tmp/rs41frames.data", "wb");
#endif

	return d;
}

void
rs41_decoder_deinit(RS41Decoder *d)
{
	framer_deinit(&d->f);
	rs_deinit(&d->rs);
	free(d);
#ifndef NDEBUG
	if (debug) fclose(debug);
#endif
}

ParserStatus
rs41_decode(RS41Decoder *self, SondeData *dst, const float *src, size_t len)
{
	RS41Subframe *subframe;
	size_t frame_offset, frame_data_len;
	uint8_t *const raw_frame = (uint8_t*)self->raw_frame;

	/* Read a new frame */
	switch (framer_read(&self->f, raw_frame, &self->offset, RS41_FRAME_LEN, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}

	/* Descramble and error correct */
	rs41_frame_descramble(&self->frame, self->raw_frame);
	rs41_frame_correct(&self->frame, &self->rs);

	/* Copy residual bits from the previous frame */
	self->raw_frame[0] = self->raw_frame[1];

#ifndef NDEBUG
	if (debug) {
		/* Output the frame to file */
		fwrite(&self->frame, RS41_FRAME_LEN/8, 1, debug);
	}
#endif

	/* Prepare to parse subframes */
	dst->fields = 0;
	frame_offset = 0;

	/* Parse expected data length from extended flag */
	frame_data_len = RS41_DATA_LEN + (rs41_frame_is_extended(&self->frame) ? RS41_XDATA_LEN : 0);

	/* Initialize pointer to the subframe */
	subframe = (RS41Subframe*)&self->frame.data[frame_offset];
	frame_offset += subframe->len + 4;

	/* Keep going until the end of the frame is reached */
	while (frame_offset < frame_data_len && subframe->len) {
		/* Validate the subframe's checksum against the one received. If it
		 * doesn't match, discard it */
		if (crc16_ccitt_false(subframe->data, subframe->len) == *(uint16_t*)&subframe->data[subframe->len]) {
			rs41_parse_subframe(dst, subframe, &self->metadata);
		}

		/* Update pointer to the subframe */
		subframe = (RS41Subframe*)&self->frame.data[frame_offset];
		frame_offset += subframe->len + 4;
	}
	/* }}} */

	return PARSED;
}

/* Static functions {{{ */
static void
rs41_update_metadata(RS41Metadata *m, RS41Subframe_Info *s)
{
	size_t frag_offset;

	/* Copy the fragment and update the bitmap of the fragments left */
	frag_offset = s->frag_seq * LEN(s->frag_data);
	memcpy((uint8_t*)&m->data + frag_offset, s->frag_data, LEN(s->frag_data));
	m->bitmask[s->frag_seq/8] |= (1 << (7 - s->frag_seq%8));
}

static float
rs41_get_calib_percent(RS41Metadata *m)
{
	float num_received;

	/* Check if we have all the sub-segments populated */
	num_received = count_ones(m->bitmask, sizeof(m->bitmask));

	return (num_received * 100.0) / RS41_CALIB_FRAGCOUNT;
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

static void
rs41_parse_subframe(SondeData *dst, RS41Subframe *subframe, RS41Metadata *metadata)
{
	RS41Subframe_Info *status = (RS41Subframe_Info*)subframe;
	RS41Subframe_PTU *ptu = (RS41Subframe_PTU*)subframe;
	RS41Subframe_GPSInfo *gpsinfo = (RS41Subframe_GPSInfo*)subframe;
	RS41Subframe_GPSPos *gpspos = (RS41Subframe_GPSPos*)subframe;
	RS41Subframe_XDATA *xdata = (RS41Subframe_XDATA*)subframe;

	uint16_t burstkill_timer;
	float x, y, z, dx, dy, dz;

	switch (subframe->type) {
	case RS41_SFTYPE_EMPTY:
		/* Padding */
		break;
	case RS41_SFTYPE_INFO:
		/* Frame sequence number, serial no., board info, calibration data */
		status = (RS41Subframe_Info*)subframe;
		rs41_update_metadata(metadata, status);

		dst->fields |= DATA_SERIAL;
		strncpy(dst->serial, status->serial, sizeof(dst->serial)-1);
		dst->serial[8] = 0;

		dst->fields |= DATA_SEQ;
		dst->seq = status->frame_seq;

		burstkill_timer = metadata->data.burstkill_timer;
		if (burstkill_timer != 0xFFFF) {
			dst->fields |= DATA_SHUTDOWN;
			dst->shutdown = burstkill_timer;
		}
		break;

	case RS41_SFTYPE_PTU:
		/* Temperature, humidity, pressure */
		ptu = (RS41Subframe_PTU*)subframe;

		dst->fields |= DATA_PTU;
		dst->temp = rs41_subframe_temp(ptu, &metadata->data);
		dst->rh = rs41_subframe_humidity(ptu, &metadata->data);
		dst->pressure = rs41_subframe_pressure(ptu, &metadata->data);
		dst->calib_percent = rs41_get_calib_percent(metadata);
		dst->calibrated = rs41_metadata_ptu_calibrated(metadata);
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


		dst->fields |= DATA_POS | DATA_SPEED;
		ecef_to_lla(&dst->lat, &dst->lon, &dst->alt, x, y, z);
		ecef_to_spd_hdg(&dst->speed, &dst->heading, &dst->climb,
				dst->lat, dst->lon, dx, dy, dz);

		break;
	case RS41_SFTYPE_GPSINFO:
		/* GPS date/time and RSSI */
		gpsinfo = (RS41Subframe_GPSInfo*)subframe;

		dst->fields |= DATA_TIME;
		dst->time = gps_time_to_utc(gpsinfo->week, gpsinfo->ms);
		break;
	case RS41_SFTYPE_XDATA:
		/* XDATA */
		xdata = (RS41Subframe_XDATA*)subframe;

		/* If pressure is not provided, estimate it from altitude so that ozone
		 * ppb calculation can still happen */
		if (!(dst->pressure > 0)) dst->pressure = altitude_to_pressure(dst->alt);

		dst->fields |= DATA_XDATA;
		rs41_xdata_decode(&dst->xdata, dst->pressure, xdata->ascii_data, xdata->len-1);
		break;
	default:
		/* Unknown */
		break;
	}
}

static void
rs41_xdata_decode(SondeXdata *dst, float curPressure, const char *asciiData, int len)
{
	unsigned int r_pumpTemp, r_o3Current, r_battVoltage, r_pumpCurrent, r_extVoltage;
	float pumpTemp, o3Current;
	unsigned int instrumentID, instrumentNum;

	while (len > 0) {
		sscanf(asciiData, "%02X%02X", &instrumentID, &instrumentNum);
		asciiData += 4;
		len -= 4;

		switch (instrumentID) {
		case RS41_XDATA_ENSCI_OZONE:
			if (sscanf(asciiData, "%04X%05X%02X%03X%02X",
			           &r_pumpTemp, &r_o3Current, &r_battVoltage, &r_pumpCurrent, &r_extVoltage) == 5) {
				asciiData += 16;
				pumpTemp = (r_pumpTemp & 0x8000 ? -1 : 1) * 0.001 * (r_pumpTemp & 0x7FFF) + 273.15;
				o3Current = r_o3Current * 1e-5;

				dst->o3_ppb = xdata_ozone_ppb(curPressure, o3Current, DEFAULT_O3_FLOWRATE, pumpTemp);
			} else {
				/* Diagnostic data */
				asciiData += 17;
			}
			break;
		default:
			break;
		}
	}
}
/* }}} */
