#include <stdio.h>
#include <stdlib.h>
#include "rs41.h"
#include "utils.h"

FILE *debug;

void
rs41_decoder_init(RS41Decoder *d, int samplerate)
{
	correlator_init(&d->correlator, RS41_SYNCWORD, RS41_SYNCLEN);
	gfsk_init(&d->gfsk, samplerate, RS41_BAUDRATE);

	debug = fopen("/tmp/rs41frames.data", "wb");
}

int
rs41_decode(RS41Decoder *d, int (*read)(float *dst))
{
	int offset;
	int inverted;
	int i;

	/* Read a frame worth of bits */
	if (!gfsk_decode(&d->gfsk, d->raw_frame, 0, RS41_FRAME_LEN*8, read)) return 0;

	/* Find the offset with the strongest correlation with the sync marker */
	offset = correlate(&d->correlator, &inverted, d->raw_frame, RS41_FRAME_LEN);

	if (offset) {
		/* Read more bits to compensate for the frame offset */
		if (!gfsk_decode(&d->gfsk, d->raw_frame + RS41_FRAME_LEN, 0, offset, read))
			return 0;
		/* Move bits to the beginning */
		bitcpy(d->raw_frame, d->raw_frame, offset, 8*RS41_FRAME_LEN);
	}

	/* De-rotate if necessary */
	if (inverted) {
		for (i=0; i<RS41_FRAME_LEN; i++) {
			d->raw_frame[i] ^= 0xFF;
		}
	}

	/* Output the frame to file */
	fwrite(d->raw_frame, RS41_FRAME_LEN, 1, debug);

	return 1;
}

void
rs41_decoder_deinit(RS41Decoder *d)
{
	fclose(debug);
	gfsk_deinit(&d->gfsk);

}
