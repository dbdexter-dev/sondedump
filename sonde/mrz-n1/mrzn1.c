#include <include/mrzn1.h>
#include <string.h>
#include "protocol.h"
#include "decode/framer.h"
#include "decode/manchester.h"


struct mrzn1decoder {
	Framer f;
	MRZN1Frame raw_frame[2];
	MRZN1Frame frame;
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
	/* Read a new frame */
	switch(framer_read(&self->f, self->raw_frame, src, len)) {
	case PROCEED:
		return PROCEED;
	case PARSED:
		break;
	}

	manchester_decode(&self->frame, self->raw_frame, MRZN1_FRAME_LEN);

#ifndef NDEBUG
	if (debug)
		fwrite(&self->frame, MRZN1_FRAME_LEN/8, 1, debug);
#endif


	dst->fields = 0;
	return PARSED;
}
