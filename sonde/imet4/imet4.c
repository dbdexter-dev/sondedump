#include <include/imet4.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "decode/framer.h"
#include "decode/manchester.h"
#include "protocol.h"
#include "frame.h"


struct imet4decoder {
	Framer f;
	IMET4Frame frame[2];
	int state;
	size_t offset;
};

enum { READ };

#ifndef NDEBUG
static FILE *debug;
#endif


IMET4Decoder*
imet4_decoder_init(int samplerate)
{
	IMET4Decoder *d = malloc(sizeof(*d));
	framer_init_afsk(&d->f, samplerate, IMET4_BAUDRATE,
			IMET4_MARK_FREQ, IMET4_SPACE_FREQ,
			IMET4_SYNCWORD, IMET4_SYNC_LEN);

	d->state = READ;
	d->offset = 0;

#ifndef NDEBUG
	debug = fopen("/tmp/imet4frames.data", "wb");
#endif

	return d;
}

void
imet4_decoder_deinit(IMET4Decoder *d) {
	framer_deinit(&d->f);
	free(d);

#ifndef NDEBUG
	if (debug) fclose(debug);
#endif

}

ParserStatus
imet4_decode(IMET4Decoder *self, SondeData *dst, const float *src, size_t len)
{
	uint8_t *const raw_frame = (uint8_t*)self->frame;
	dst->type = EMPTY;

	switch (self->state) {
		case READ:
			switch(framer_read(&self->f, raw_frame, &self->offset, IMET4_FRAME_LEN, src, len)) {
				case PROCEED:
					return PROCEED;
				case PARSED:
					break;
			}

			imet4_frame_descramble(self->frame);

#ifndef NDEBUG
			fwrite(raw_frame, IMET4_FRAME_LEN/8, 1, debug);
#endif
			break;
		default:
			break;
	}

	return PARSED;
}
