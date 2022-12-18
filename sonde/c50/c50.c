#include <include/c50.h>
#include <string.h>
#include "protocol.h"
#include "decode/framer.h"


struct c50decoder {
	Framer f;
	C50Frame raw_frame[2];
	C50Frame frame;
	size_t offset;
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

	d->offset = 0;

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
	uint8_t *const raw_frame = (uint8_t*)self->raw_frame;

	/* Read a new frame */
	switch(framer_read(&self->f, raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}

#ifndef NDEBUG
	if (debug && !memcmp(raw_frame, "\x00\xff", 2))
		fwrite(raw_frame, C50_FRAME_LEN/8, 1, debug);
#endif

	self->raw_frame[0] = self->raw_frame[1];
	dst->fields = 0;
	return PARSED;
}
