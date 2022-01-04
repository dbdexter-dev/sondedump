#include <stdio.h>
#include "decode/framer.h"
#include "decode/manchester.h"
#include "frame.h"
#include "ims100.h"
#include "protocol.h"
#include "utils.h"

#ifndef NDEBUG
static FILE *debug;
#endif

enum { READ, PARSE };

void
ims100_decoder_init(IMS100Decoder *d, int samplerate)
{
	gfsk_init(&d->gfsk, samplerate, IMS100_BAUDRATE);
	correlator_init(&d->correlator, IMS100_SYNCWORD, IMS100_SYNC_LEN);
	d->state = READ;
#ifndef NDEBUG
	debug = fopen("/tmp/ims100frames.data", "wb");
#endif
}

void
ims100_decoder_deinit(IMS100Decoder *d)
{
	gfsk_deinit(&d->gfsk);
#ifndef NDEBUG
	fclose(debug);
#endif
}

SondeData
ims100_decode(IMS100Decoder *self, int (*read)(float *dst))
{
	SondeData data = {.type = EMPTY};
	uint8_t *const raw_frame = (uint8_t*)self->frame;

	switch (self->state) {
		case READ:
			/* Read a new frame */
			if (read_frame_gfsk(&self->gfsk, &self->correlator, raw_frame, read, IMS100_FRAME_LEN, 0) < 0) {
				data.type = SOURCE_END;
				return data;
			}

			manchester_decode(raw_frame, raw_frame, IMS100_FRAME_LEN);
			ims100_frame_descramble(self->frame);

#ifndef NDEBUG
			fwrite(raw_frame, IMS100_FRAME_LEN/2/8, 1, debug);
			fflush(debug);
#endif

			break;
		default:
			break;
	}

	return data;
}

