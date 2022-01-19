#include "bitops.h"
#include "framer.h"

enum { READ, REALIGN } state;

int
framer_init_gfsk(Framer *f, int samplerate, int baudrate, uint64_t syncword, int synclen)
{
	gfsk_init(&f->gfsk, samplerate, baudrate);
	correlator_init(&f->corr, syncword, synclen);
	f->state = READ;

	return 0;
}

void
framer_deinit(Framer *f)
{
	gfsk_deinit(&f->gfsk);
}

ParserStatus
read_frame_gfsk(Framer *f, uint8_t *dst, size_t *bit_offset, size_t framelen, const float *src, size_t len)
{
	int i;

	switch (f->state) {
		case READ:
			switch (gfsk_demod(&f->gfsk, dst, bit_offset, framelen, src, len)) {
				case PROCEED:
					return PROCEED;
				case PARSED:
					break;
			}

			/* Find offset of the sync marker */
			f->sync_offset = correlate(&f->corr, &f->inverted, dst, framelen/8);
			f->offset = framelen;
			*bit_offset = f->sync_offset;

			f->state = REALIGN;
			/* FALLTHROUGH */
		case REALIGN:
			/* Read more bits to get a full frame worth */
			switch (gfsk_demod(&f->gfsk, dst, &f->offset, framelen + f->sync_offset, src, len)) {
				case PROCEED:
					return PROCEED;
				case PARSED:
					break;
			}

			/* Realign frame to the beginning of the buffer */
			if (f->sync_offset) bitcpy(dst, dst, f->sync_offset, framelen);

			/* Undo inversion */
			if (f->inverted) {
				for (i=0; i < ((int)framelen - 7)/8 + 1; i++) {
					dst[i] ^= 0xFF;
				}
			}

			f->state = READ;
			return PARSED;
	}

	return PROCEED;
}
