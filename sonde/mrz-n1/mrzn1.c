#include <include/mrzn1.h>
#include <string.h>
#include "bitops.h"
#include "decode/framer.h"
#include "decode/manchester.h"
#include "frame.h"
#include "gps/ecef.h"
#include "log/log.h"
#include "parser.h"
#include "physics.h"
#include "protocol.h"

typedef struct {
	MRZN1Calibration data;
	uint16_t bitmask;
} MRZN1Metadata;

static void update_calibration(MRZN1Metadata *calib, int seq, uint8_t *data);
static void mrzn1_parse_frame(SondeData *dst, const MRZN1Frame *frame, const MRZN1Metadata *calib);

struct mrzn1decoder {
	Framer f;
	MRZN1RawFrame raw_frame[2];
	MRZN1Frame frame;
	MRZN1Metadata meta;

	size_t offset;
};

#ifndef NDEBUG
static FILE *debug;
#endif

MRZN1Decoder*
mrzn1_decoder_init(int samplerate)
{
	MRZN1Decoder *d = malloc(sizeof(*d));
	framer_init_gfsk(&d->f, samplerate, MRZN1_BAUDRATE, MRZN1_FRAME_LEN,
			MRZN1_SYNCWORD, MRZN1_SYNC_LEN);

	d->offset = 0;
	memset(&d->meta, 0, sizeof(d->meta));

#ifndef NDEBUG
	debug = fopen("/tmp/mrzn1frames.data", "wb");
#endif

	return d;
}

void
mrzn1_decoder_deinit(MRZN1Decoder *d) {
	framer_deinit(&d->f);
	free(d);

#ifndef NDEBUG
	if (debug) fclose(debug);
#endif
}

ParserStatus
mrzn1_decode(MRZN1Decoder *self, SondeData *dst, const float *src, size_t len)
{
	int errcount;

	/* Read a new frame */
	switch(framer_read(&self->f, self->raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}

	manchester_decode(&self->frame, self->raw_frame, MRZN1_FRAME_LEN);
	errcount = mrzn1_frame_correct(&self->frame);

#ifndef NDEBUG
	if (debug)
		fwrite(&self->frame, sizeof(self->frame), 1, debug);
#endif
	dst->fields = 0;

	if (errcount >= 0) {
		update_calibration(&self->meta, mrzn1_calib_seq(&self->frame), self->frame.calib_data);
		mrzn1_parse_frame(dst, &self->frame, &self->meta);
	}

	return PARSED;
}

/* Static functions {{{ */
static void
mrzn1_parse_frame(SondeData *dst, const MRZN1Frame *frame, const MRZN1Metadata *calib)
{
	float x, y, z, dx, dy, dz;

	/* Parse seq */
	dst->seq = mrzn1_seq(frame);
	dst->fields |= DATA_SEQ;

	/* Parse time */
	dst->time = mrzn1_time(frame, &calib->data);
	dst->fields |= DATA_TIME;

	/* Parse GPS coordinates */
	x = mrzn1_x(frame);
	y = mrzn1_y(frame);
	z = mrzn1_z(frame);
	dx = mrzn1_dx(frame);
	dy = mrzn1_dy(frame);
	dz = mrzn1_dz(frame);

	/* Convert cartesian position to lat/lon/alt */
	ecef_to_lla(&dst->lat, &dst->lon, &dst->alt, x, y, z);
	dst->fields |= DATA_POS;

	/* Convert cartesian speed to speed/heading/climb */
	ecef_to_spd_hdg(&dst->speed, &dst->heading, &dst->climb,
					dst->lat, dst->lon, dx, dy, dz);
	dst->fields |= DATA_SPEED;

	/* Parse PTU data */
	dst->calib_percent = 100.0f * count_ones(&calib->bitmask, 2) / MRZN1_CALIB_FRAGCOUNT;
	dst->temp = mrzn1_temp(frame);
	dst->rh = mrzn1_rh(frame);
	dst->pressure = altitude_to_pressure(dst->alt);
	dst->fields |= DATA_PTU;

	log_debug_hexdump(&calib->data, sizeof(calib->data));

	if (BITMASK_CHECK(calib->bitmask, 0x4)) {
		mrzn1_serial(dst->serial, &calib->data);
		dst->fields |= DATA_SERIAL;
	}
}

static void
update_calibration(MRZN1Metadata *calib, int seq, uint8_t *data)
{
	memcpy(((uint8_t*)&calib->data) + MRZN1_CALIB_FRAGSIZE * seq, data, MRZN1_CALIB_FRAGSIZE);
	calib->bitmask |= 1 << (MRZN1_CALIB_FRAGCOUNT - seq - 1);
}
/* }}} */
