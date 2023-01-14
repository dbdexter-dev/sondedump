#include <assert.h>
#include <string.h>
#include "bitops.h"
#include "framer.h"
#include "log/log.h"

static ParserStatus framer_demod_internal(Framer *f, void *dst, size_t *bit_offset, size_t framelen, const float *src, size_t len);

enum { READ_PRE, READ, REALIGN } state;

int
framer_init_gfsk(Framer *f, int samplerate, int baudrate, size_t framelen, uint64_t syncword, int synclen)
{
	f->type = GFSK;
	if (gfsk_init(&f->demod.gfsk, samplerate, baudrate)) return 1;
	correlator_init(&f->corr, syncword, synclen);
	f->state = READ;
	f->offset = 0;
	f->framelen = framelen;

	return 0;
}

int
framer_init_afsk(Framer *f, int samplerate, int baudrate, size_t framelen, float f_mark, float f_space, uint64_t syncword, int synclen)
{
	f->type = AFSK;
	if (afsk_init(&f->demod.afsk, samplerate, baudrate, f_mark, f_space)) return 1;
	correlator_init(&f->corr, syncword, synclen);
	f->state = READ;
	f->offset = 0;
	f->framelen = framelen;

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
framer_read(Framer *f, void *v_dst, const float *src, size_t len)
{
	uint8_t *dst = v_dst;
	int i;

	switch (f->state) {
	case READ_PRE:
		/* Copy bits from the previous frame */
		assert(f->offset >= f->framelen);
		f->offset -= f->framelen;

		if (f->framelen % 8) {
			bitcpy(dst, dst, f->framelen, f->offset);
		} else {
			memcpy(dst, dst + f->framelen/8, f->offset/8+1);
		}

		f->state = READ;
		/* FALLTHROUGH */
	case READ:
		switch (framer_demod_internal(f, dst, &f->offset, f->framelen + f->corr.sync_len, src, len)) {
		case PROCEED:
			return PROCEED;
		case PARSED:
			break;
		}

		/* Find offset of the sync marker */
		f->sync_offset = correlate(&f->corr, &f->inverted, dst, f->framelen/8);
		assert(f->sync_offset >= 0);
		log_debug("Offset %d", f->sync_offset);

		f->state = REALIGN;
		/* FALLTHROUGH */
	case REALIGN:
		/* Conditionally read more bits to get a full frame worth of bits */
		switch (framer_demod_internal(f, dst, &f->offset, f->framelen + f->sync_offset, src, len)) {
		case PROCEED:
			return PROCEED;
		case PARSED:
			break;
		}

		/* Realign frame to the beginning of the buffer */
		if (f->sync_offset) bitcpy(dst, dst, f->sync_offset, f->framelen);

		/* Undo inversion */
		if (f->inverted) {
			for (i=0; i < (int)f->framelen/8; i++) {
				dst[i] ^= 0xFF;
			}
			/* Handle last few bits separately */
			if (f->framelen % 8)
				dst[i] ^= ~((1 << (8 - (f->framelen % 8))) - 1);
		}

		f->state = READ_PRE;
		return PARSED;
	}

	return PROCEED;
}

void
framer_adjust(Framer *f, void *v_dst, size_t bits_delta)
{
	const size_t valid_bits = f->framelen;
	uint8_t *dst = v_dst;
	size_t i;

	assert(bits_delta <= valid_bits);

	/* Shift bits by the specified amount */
	bitcpy(dst, dst, bits_delta, valid_bits - bits_delta);

	/* De-invert if necessary */
	if (f->inverted) {
		for (i=0; i<(valid_bits - bits_delta)/8+1; i++) {
			dst[i] ^= 0xFF;
		}
	}

	/* Update the bit offset to reflect the number of valid bits in the buffer */
	f->offset = valid_bits - bits_delta;

	/* Skip the next READ_PRE step */
	f->state = READ;
}

/* Static functions {{{ */
static ParserStatus
framer_demod_internal(Framer *f, void *dst, size_t *bit_offset, size_t framelen, const float *src, size_t len)
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
/* }}} */
