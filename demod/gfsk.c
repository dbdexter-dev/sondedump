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

	/* Initialize AGC */
	agc_init(&g->agc);

	/* Initialize a low-pass filter with the appropriate bandwidth */
	if (filter_init_lpf(&g->lpf, GFSK_FILTER_ORDER, sym_freq)) return 1;

	/* Initialize symbol timing recovery */
	timing_init(&g->timing, sym_freq, SYM_ZETA, sym_freq/250);

	g->src_offset = 0;
	g->dst_offset = 0;

	return 0;
}

void
gfsk_deinit(GFSKDemod *g)
{
	filter_deinit(&g->lpf);
}

ParserStatus
gfsk_demod(GFSKDemod *g, uint8_t *dst, size_t *bit_offset, size_t count, const float *src, size_t len)
{
	size_t local_bit_offset = *bit_offset;
	float symbol;
	float interm;
	uint8_t tmp;

	/* Normalize bit offset */
	dst += local_bit_offset/8;
	count -= local_bit_offset;
	local_bit_offset %= 8;

	/* Initialize first byte that will be touched */
	tmp = *dst >> (8 - local_bit_offset);
	interm = 0;

	while (count > 0) {
		/* If new read would be out of bounds, ask the reader for more */
		if (g->src_offset >= len) {
			*bit_offset = *bit_offset - *bit_offset%8 + local_bit_offset;
			g->src_offset = 0;
			return PROCEED;
		}

		/* Apply AGC and pass to filter */
		symbol = agc_apply(&g->agc, src[g->src_offset++]);
		filter_fwd_sample(&g->lpf, symbol);

		/* Recover symbol value */
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
				local_bit_offset++;
				count--;

				/* If a byte boundary is crossed, write to dst */
				if (!(local_bit_offset % 8)) {
					*dst++ = tmp;
					tmp = 0;
				}
				break;
			default:
				break;

		}
	}

	/* Last write */
	if (local_bit_offset%8) *dst = (tmp << (8 - (local_bit_offset % 8)));
	*bit_offset = 0;
	return PARSED;
}
