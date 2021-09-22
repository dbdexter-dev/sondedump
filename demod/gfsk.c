#include <math.h>
#include "dsp/agc.h"
#include "dsp/filter.h"
#include "dsp/timing.h"
#include "gfsk.h"

static int _samplerate;
static int _symrate;
static Filter _lpf;

int
gfsk_init(int samplerate, int symrate)
{
	const float sym_freq = 2.0*M_PI*symrate/samplerate;

	/* Save input settings */
	_samplerate = samplerate;
	_symrate = symrate;

	/* Initialize a low-pass filter with the appropriate bandwidth */
	if (filter_init_lpf(&_lpf, GFSK_FILTER_ORDER, sym_freq)) return 1;

	/* Initialize symbol timing recovery */
	timing_init(sym_freq, SYM_BW);

	return 0;
}

void
gfsk_deinit()
{
	filter_deinit(&_lpf);
}

int
gfsk_decode(uint8_t *dst, int bit_offset, size_t len, int (*read)(float *dst, size_t len))
{
	float symbol;
	uint8_t tmp;

	/* Normalize bit offset */
	dst += bit_offset/8;
	bit_offset %= 8;

	/* Initialize first byte that will be touched */
	if (bit_offset) {
		*dst &= ~((1 << bit_offset) - 1);
	} else {
		*dst = 0;
	}

	tmp = 0;
	while (len > 0) {
		/* Read a new sample and filter it */
		if (!read(&symbol, 1)) break;
		filter_fwd_sample(&_lpf, symbol);

		/* Check if we should sample a new symbol at this time */
		if (advance_timeslot()) {
			symbol = filter_get(&_lpf);
			retime(symbol);

			/* Slice to get bit value */
			tmp = (tmp << 1) | (symbol > 0);
			bit_offset++;
			len--;

			/* If a byte boundary is crossed, write to dst */
			if (!(bit_offset % 8)) {
				*dst++ |= tmp;
				tmp = 0;
				*dst = 0;
			}
		}
	}

	/* Last write */
	if (!(bit_offset % 8)) {
		*dst |= (tmp << (7 - (bit_offset % 8)));
	}

	return bit_offset;
}
