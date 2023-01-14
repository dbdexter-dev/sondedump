#include <include/c50.h>
#include <string.h>
#include "frame.h"
#include "protocol.h"
#include "decode/framer.h"


struct c50decoder {
	Framer f;
	C50RawFrame raw_frame[2];
	C50Frame frame;
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
	/* Read a new frame */
	switch(framer_read(&self->f, self->raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}

	c50_frame_descramble(&self->frame, self->raw_frame);

	dst->fields = 0;

	/* If frame contains errors, do not parse */
	if (c50_frame_correct(&self->frame) < 0) {
		return PARSED;
	}

#ifndef NDEBUG
		fwrite(&self->frame, sizeof(self->frame), 1, debug);
#endif


	return PARSED;
}
