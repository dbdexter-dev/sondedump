#include <math.h>
#include <stdio.h>
#include "dsp/agc.h"
#include "gfsk.h"
#include "utils.h"

int
gfsk_init(GFSKDemod *g, int samplerate, int symrate)
{
	const float sym_freq = (float)symrate/samplerate;

	/* Save input settings */
	g->samplerate = samplerate;
	g->symrate = symrate;

	/* Initialize a low-pass filter with the appropriate bandwidth */
	if (filter_init_lpf(&g->lpf, GFSK_FILTER_ORDER, sym_freq)) return 1;

	/* Initialize symbol timing recovery */
	timing_init(&g->timing, sym_freq, SYM_ALPHA);

	return 0;
}

void
gfsk_deinit(GFSKDemod *g)
{
	filter_deinit(&g->lpf);
}

int
gfsk_demod(GFSKDemod *g, uint8_t *dst, int bit_offset, size_t len, int (*read)(float *dst))
{
	float symbol;
	float interm;
	uint8_t tmp;

	/* Normalize bit offset */
	dst += bit_offset/8;
	bit_offset %= 8;

	/* Initialize first byte that will be touched */
	tmp = (*dst & ~((1 << bit_offset) - 1)) >> bit_offset;
	interm = 0;
	while (len > 0) {
		/* Read a new sample and filter it */
		if (!read(&symbol)) break;
		symbol = agc_apply(symbol);
		filter_fwd_sample(&g->lpf, symbol);

		switch (advance_timeslot(&g->timing)) {
			case 1:
				/* Half-way slot */
				interm = filter_get(&g->lpf);
				break;
			case 2:
				/* Correct slot: update time estimate */
				symbol = filter_get(&g->lpf);
				retime(&g->timing, interm, symbol);

				/* Slice sample to get bit value */
				tmp = (tmp << 1) | (symbol > 0 ? 1 : 0);
				bit_offset++;
				len--;

				/* If a byte boundary is crossed, write to dst */
				if (!(bit_offset % 8)) {
					*dst++ = tmp;
					tmp = 0;
				}
				break;
			default:
				break;

		}
	}

	/* Last write */
	*dst = (tmp << (7 - (bit_offset % 8)));

	return bit_offset;
}
