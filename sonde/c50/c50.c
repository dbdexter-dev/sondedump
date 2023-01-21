#include <include/c50.h>
#include <string.h>
#include <time.h>
#include "bitops.h"
#include "frame.h"
#include "protocol.h"
#include "decode/framer.h"
#include "log/log.h"
#include "physics.h"
#include "parser.h"

struct c50decoder {
	Framer f;
	C50RawFrame raw_frame[2];
	C50Frame frame;

	SondeData partial_dst;
	struct tm time;
};

#ifndef NDEBUG
static FILE *debug;
#endif

C50Decoder*
c50_decoder_init(int samplerate)
{
	time_t zero = 0;

	C50Decoder *d = malloc(sizeof(*d));
	framer_init_afsk(&d->f, samplerate, C50_BAUDRATE, C50_FRAME_LEN,
			C50_MARK_FREQ, C50_SPACE_FREQ,
			C50_SYNCWORD, C50_SYNC_LEN);

	memset(&d->partial_dst, 0, sizeof(d->partial_dst));
	memset(&d->time, 0, sizeof(d->time));
	gmtime_r(&zero, &d->time);

#ifndef NDEBUG
	debug = fopen("/tmp/c50frames.data", "wb");
#endif

	return d;
}

void
c50_decoder_deinit(C50Decoder *d) {
	framer_deinit(&d->f);
	free(d);

#ifndef NDEBUG
	if (debug) fclose(debug);
#endif
}

ParserStatus
c50_decode(C50Decoder *self, SondeData *dst, const float *src, size_t len)
{
	/* Read a new frame */
	switch(framer_read(&self->f, self->raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}

	c50_frame_descramble(&self->frame, self->raw_frame);

#ifndef NDEBUG
	fwrite(&self->raw_frame[0], 2*sizeof(self->frame), 1, debug);
#endif


	dst->fields = 0;

	/* If frame contains errors, do not parse */
	if (c50_frame_correct(&self->frame) < 0) {
		return PARSED;
	}

	//log_debug_hexdump(&self->frame, sizeof(self->frame));

	switch (self->frame.type) {
	case C50_TYPE_TEMP_AIR:
		self->partial_dst.calib_percent = 100.0f;
		self->partial_dst.temp = c50_temp(&self->frame);
		self->partial_dst.fields |= DATA_PTU;
		break;

	case C50_TYPE_RH:
		self->partial_dst.rh = ieee754_be(self->frame.data);
		break;

	case C50_TYPE_DATE:
		c50_date(&self->time, &self->frame);
		break;

	case C50_TYPE_TIME:
		c50_time(&self->time, &self->frame);

		self->partial_dst.fields |= DATA_TIME;
		self->partial_dst.time = my_timegm(&self->time);
		break;

	case C50_TYPE_LAT:
		self->partial_dst.lat = c50_lat(&self->frame);
		break;

	case C50_TYPE_LON:
		self->partial_dst.lon = c50_lon(&self->frame);
		break;

	case C50_TYPE_ALT:
		self->partial_dst.alt = c50_alt(&self->frame);
		self->partial_dst.fields |= DATA_POS;
		self->partial_dst.pressure = altitude_to_pressure(self->partial_dst.alt);
		break;

	case C50_TYPE_SN:
		c50_serial(self->partial_dst.serial, &self->frame);
		self->partial_dst.fields |= DATA_SERIAL;
		break;

	default:
		log_debug("Unknown frame type: %d", self->frame.type);
		log_debug_hexdump(&self->frame, sizeof(self->frame));
		break;
	}

	/* Copy from internal data buffer to output */
	memcpy(dst, &self->partial_dst, sizeof(*dst));
	self->partial_dst.fields = 0;

	return PARSED;
}
