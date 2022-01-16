#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "dsp/agc.h"
#include "gfsk.h"
#include "utils.h"

int
gfsk_init(GFSKDemod *g, int samplerate, int symrate)
{
	const float sym_freq = (float)symrate/samplerate;
	const int num_phases = 1 + (MIN_SAMPLES_PER_SYMBOL * sym_freq);

	/* Save input settings */
	g->samplerate = samplerate;
	g->symrate = symrate;

	/* Initialize AGC */
	agc_init(&g->agc);

	/* Initialize a low-pass filter with the appropriate bandwidth */
	if (filter_init_lpf(&g->lpf, GFSK_FILTER_ORDER, sym_freq, num_phases)) return 1;

	/* Initialize symbol timing recovery */
	timing_init(&g->timing, sym_freq / num_phases, SYM_ZETA, sym_freq/num_phases/250);

	g->src_offset = 0;

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
	float symbol;
	uint8_t tmp;
	int phase;

	/* Normalize bit offset */
	dst += *bit_offset/8;
	count -= *bit_offset;

	/* Initialize first byte that will be touched */
	tmp = *dst >> (8 - *bit_offset%8);
	g->interm = 0;

	while (count > 0) {
		/* If new read would be out of bounds, ask the reader for more */
		if (g->src_offset >= len) {
			if (*bit_offset%8) *dst = (tmp << (8 - (*bit_offset % 8)));
			g->src_offset = 0;
			return PROCEED;
		}

		/* Apply AGC and pass to filter */
		symbol = agc_apply(&g->agc, src[g->src_offset++]);
		filter_fwd_sample(&g->lpf, symbol);

		/* Recover symbol value */
		for (phase = 0; phase < g->lpf.num_phases; phase++)  {
			switch (advance_timeslot(&g->timing)) {
				case 1:
					/* Half-way slot */
					g->interm = filter_get(&g->lpf, phase);
					break;
				case 2:
					/* Correct slot: update time estimate */
					symbol = filter_get(&g->lpf, phase);
					retime(&g->timing, g->interm, symbol);

					/* Slice sample to get bit value */
					tmp = (tmp << 1) | (symbol > 0 ? 1 : 0);
					(*bit_offset)++;
					count--;

					/* If a byte boundary is crossed, write to dst */
					if (!(*bit_offset % 8)) {
						*dst++ = tmp;
						tmp = 0;
					}
					break;
				default:
					break;

			}
		}
	}

	/* Last write */
	if (*bit_offset%8) *dst = (tmp << (8 - (*bit_offset % 8)));
	*bit_offset = 0;
	return PARSED;
}
