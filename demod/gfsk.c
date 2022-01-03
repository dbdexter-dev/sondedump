#include <math.h>
#include <stdio.h>
#include "dsp/agc.h"
#include "gfsk.h"
#include "utils.h"

#define NDEBUG

#ifndef NDEBUG
static FILE *debug;
#endif

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

#ifndef NDEBUG
	debug = fopen("/tmp/demod.data", "wb");
#endif

	return 0;
}

void
gfsk_deinit(GFSKDemod *g)
{
	filter_deinit(&g->lpf);
#ifndef NDEBUG
	if (debug) {
		fclose(debug);
		debug = NULL;
	}
#endif
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
	tmp = *dst >> (8 - bit_offset);
	interm = 0;
	while (len > 0) {
		/* Read a new sample and filter it */
		if (!read(&symbol)) break;
		symbol = agc_apply(&g->agc, symbol);
		filter_fwd_sample(&g->lpf, symbol);

#ifndef NDEBUG
		fprintf(debug, "%f ", filter_get(&g->lpf));
#endif

		switch (advance_timeslot(&g->timing)) {
			case 1:
				/* Half-way slot */
				interm = filter_get(&g->lpf);
#ifndef NDEBUG
				fprintf(debug, "0\n");
#endif
				break;
			case 2:
				/* Correct slot: update time estimate */
				symbol = filter_get(&g->lpf);
				retime(&g->timing, interm, symbol);
#ifndef NDEBUG
				fprintf(debug, "%f\n", symbol);
#endif

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
#ifndef NDEBUG
				fprintf(debug, "0\n");
#endif
				break;

		}
	}

	/* Last write */
	if (bit_offset%8) *dst = (tmp << (8 - (bit_offset % 8)));

	return bit_offset;
}
