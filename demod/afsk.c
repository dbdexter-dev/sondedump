#include <math.h>
#include "afsk.h"

int
afsk_init(AFSKDemod *d, int samplerate, int symrate, int f_mark, int f_space)
{
	const float sym_freq = (float)symrate/samplerate;
	const int num_phases = 1 + (MIN_SAMPLES_PER_SYMBOL * sym_freq);

	/* Save input settings */
	d->f_mark = f_mark / samplerate;
	d->f_space = f_space / samplerate;

	/* Initialize a low-pass filter with the appropriate bandwidth */
	if (filter_init_lpf(&d->lpf, AFSK_FILTER_ORDER, sym_freq, num_phases)) return 1;

	/* Initialize symbol timing recovery */
	timing_init(&d->timing, sym_freq/num_phases, AFSK_SYM_ZETA, sym_freq/num_phases/100);

	d->p_mark = 0;
	d->p_space = 0;
	d->idx = 0;
	d->len = 1.0 / sym_freq;

	d->mark_history = malloc(sizeof(*d->mark_history) * d->len);
	d->space_history = malloc(sizeof(*d->mark_history) * d->len);

	return 0;
}

void
afsk_deinit(AFSKDemod *d)
{
	free(d->mark_history);
	free(d->space_history);
	filter_deinit(&d->lpf);
}

ParserStatus
afsk_demod(AFSKDemod *const d, uint8_t *dst, size_t *bit_offset, size_t count, const float *src, size_t len)
{
	float out_mark, out_space;
	float out_i, out_q;
	float symbol;
	uint8_t tmp;
	int phase;

	float p_mark = d->p_mark;
	float p_space = d->p_space;
	float mark_sum = d->mark_sum;
	float space_sum = d->space_sum;

	/* Normalize bit offset */
	dst += *bit_offset/8;
	count -= *bit_offset;

	/* Initialize first byte that will be touched */
	tmp = *dst >> (8 - *bit_offset%8);
	d->interm = 0;


	for (; len>0; len--) {
		/* Calculate mark and space mix output */
		out_i = *src * cosf(2 * M_PI * p_mark);
		out_q = *src * sinf(2 * M_PI * p_mark);
		out_mark = out_i*out_i + out_q*out_q;

		out_i = *src * cosf(2 * M_PI * p_space);
		out_q = *src * sinf(2 * M_PI * p_space);
		out_space = out_i*out_i + out_q*out_q;

		src++;

		/* Update moving averages */
		mark_sum += out_mark - d->mark_history[d->idx];
		space_sum += out_space - d->space_history[d->idx];

		/* Update mark and space phase */
		p_mark = fmod(p_mark + d->f_mark, 2*M_PI);
		p_space = fmod(p_space + d->f_space, 2*M_PI);

		/* Update history buffers */
		d->mark_history[d->idx] = out_mark;
		d->space_history[d->idx] = out_space;

		/* Update index */
		d->idx = (d->idx + 1) % d->len;

		/* Compute bit output signal */
		symbol = mark_sum - space_sum;

		/* Apply filter */
		filter_fwd_sample(&d->lpf, symbol);

		/* Recover symbol timing */
		for (phase = 0; phase < d->lpf.num_phases; phase++)  {
			switch (advance_timeslot(&d->timing)) {
				case 1:
					/* Half-way slot */
					d->interm = filter_get(&d->lpf, phase);
					break;
				case 2:
					/* Correct slot: update time estimate */
					symbol = filter_get(&d->lpf, phase);
					retime(&d->timing, d->interm, symbol);

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

	/* Save state */
	d->p_mark = p_mark;
	d->p_space = p_space;
	d->mark_sum = mark_sum;
	d->space_sum = space_sum;

	/* Handle last write */
	if (*bit_offset%8) *dst = (tmp << (8 - (*bit_offset % 8)));
	*bit_offset = 0;

	return PARSED;
}
