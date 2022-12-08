#include "bitops.h"
#include "framer.h"
#include "log/log.h"

static ParserStatus framer_demod_internal(Framer *f, uint8_t *dst, size_t *bit_offset, size_t framelen, const float *src, size_t len);

enum { READ, REALIGN } state;

int
framer_init_gfsk(Framer *f, int samplerate, int baudrate, uint64_t syncword, int synclen)
{
	f->type = GFSK;
	gfsk_init(&f->demod.gfsk, samplerate, baudrate);
	correlator_init(&f->corr, syncword, synclen);
	f->state = READ;

	return 0;
}

int
framer_init_afsk(Framer *f, int samplerate, int baudrate, float f_mark, float f_space, uint64_t syncword, int synclen)
{
	f->type = AFSK;
	afsk_init(&f->demod.afsk, samplerate, baudrate, f_mark, f_space);
	correlator_init(&f->corr, syncword, synclen);
	f->state = READ;

	return 0;
}

void
framer_deinit(Framer *f)
{
	switch (f->type) {
	case GFSK:
		gfsk_deinit(&f->demod.gfsk);
		break;
	case AFSK:
		afsk_deinit(&f->demod.afsk);
		break;
	}
}

ParserStatus
framer_read(Framer *f, uint8_t *dst, size_t *bit_offset, size_t framelen, const float *src, size_t len)
{
	int i;

	switch (f->state) {
	case READ:
		switch (framer_demod_internal(f, dst, bit_offset, framelen + 8*f->corr.sync_len, src, len)) {
		case PROCEED:
			return PROCEED;
		case PARSED:
			break;
		}

		/* Find offset of the sync marker */
		f->sync_offset = correlate(&f->corr, &f->inverted, dst, framelen/8);
		f->offset = framelen + 8*f->corr.sync_len;
		*bit_offset = MAX(8*(size_t)f->corr.sync_len, f->sync_offset);

		log_debug("Offset %d", f->sync_offset);

		f->state = REALIGN;
		/* FALLTHROUGH */
	case REALIGN:
		/* Conditionally read more bits to get a full frame worth of bits */
		if (f->sync_offset > (size_t)f->corr.sync_len*8) {
			switch (framer_demod_internal(f, dst, &f->offset, framelen + f->sync_offset, src, len)) {
			case PROCEED:
				return PROCEED;
			case PARSED:
				break;
			}
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

static ParserStatus
framer_demod_internal(Framer *f, uint8_t *dst, size_t *bit_offset, size_t framelen, const float *src, size_t len)
{
	switch (f->type) {
	case GFSK:
		return gfsk_demod(&f->demod.gfsk, dst, bit_offset, framelen, src, len);
	case AFSK:
		return afsk_demod(&f->demod.afsk, dst, bit_offset, framelen, src, len);
	default:
		return PROCEED;
	}
}
