#include <include/c50.h>
#include <string.h>
#include "bitops.h"
#include "frame.h"
#include "protocol.h"
#include "decode/framer.h"
#include "log/log.h"

struct c50decoder {
	Framer f;
	C50RawFrame raw_frame[2];
	C50Frame frame;

	float lat, lon, alt;
	float temp, rh;
};

#ifndef NDEBUG
static FILE *debug;
#endif

C50Decoder*
c50_decoder_init(int samplerate)
{
	C50Decoder *d = malloc(sizeof(*d));
	framer_init_afsk(&d->f, samplerate, C50_BAUDRATE, C50_FRAME_LEN,
			C50_MARK_FREQ, C50_SPACE_FREQ,
			C50_SYNCWORD, C50_SYNC_LEN);

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
	uint32_t data;

	/* Read a new frame */
	switch(framer_read(&self->f, self->raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}

	c50_frame_descramble(&self->frame, self->raw_frame);
#ifndef NDEBUG
	if (!memcmp(&self->frame, "\x00\xff", 2))
	fwrite(&self->frame, sizeof(self->frame), 1, debug);
#endif


	dst->fields = 0;

	/* If frame contains errors, do not parse */
	if (c50_frame_correct(&self->frame) < 0) {
		return PARSED;
	}

	log_debug_hexdump(&self->frame, sizeof(self->frame));
	data = bitmerge(self->frame.data, 32);


	switch (self->frame.type) {
	case C50_TYPE_TEMP_AIR:
		dst->fields |= DATA_PTU;
		dst->temp = self->temp = ieee754_be(self->frame.data);
		dst->rh = self->rh;
		break;
	case C50_TYPE_RH:
		dst->fields |= DATA_PTU;
		dst->rh = self->rh = ieee754_be(self->frame.data);
		dst->temp = self->temp;
		break;
	case C50_TYPE_DATE:
		break;
	case C50_TYPE_TIME:
		break;
	case C50_TYPE_LAT:
		dst->fields |= DATA_POS;
		dst->lat = self->lat = (int32_t)data / 1e7 + ((int32_t)data % 10000000 / 60.0 * 100.0) / 1e7;
		dst->lon = self->lon;
		dst->alt = self->alt;
		break;
	case C50_TYPE_LON:
		dst->fields |= DATA_POS;
		dst->lon = self->lon = (int32_t)data / 1e7 + ((int32_t)data % 10000000 / 60.0 * 100.0) / 1e7;
		dst->lat = self->lat;
		dst->alt = self->alt;
		break;
	case C50_TYPE_ALT:
		dst->fields |= DATA_POS;
		dst->alt = self->alt = data / 10.0f;
		dst->lat = self->lat;
		dst->lon = self->lon;
		break;

	}

	return PARSED;
}
