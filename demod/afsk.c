#include <math.h>
#include <string.h>
#include "afsk.h"
#include "utils.h"

#ifndef NDEBUG
//#define AFSK_DEBUG
#endif

#ifdef AFSK_DEBUG
#include <stdio.h>
static FILE *debug;
#endif

int
afsk_init(AFSKDemod *d, int samplerate, int symrate, float f_mark, float f_space)
{
	const float sym_freq = (float)symrate/samplerate;
	const int num_phases = 1 + (MIN_SAMPLES_PER_SYMBOL * sym_freq);

	/* Save input settings */
	d->f_mark = 2 * M_PI * f_mark / samplerate;
	d->f_space = 2 * M_PI * f_space / samplerate;

	/* Initialize symbol AGC */
	agc_init(&d->agc);

	/* Initialize a low-pass filter with the appropriate bandwidth */
	if (filter_init_lpf(&d->lpf, AFSK_FILTER_ORDER, 3 * sym_freq, num_phases)) return 1;

	/* Initialize symbol timing recovery */
	timing_init(&d->timing, sym_freq/num_phases, AFSK_SYM_ZETA, sym_freq/num_phases/100);

	d->p_mark = 0;
	d->p_space = 0;
	d->idx = 0;
	d->len = 1.0 / sym_freq;
#ifndef _MSC_VER
	d->mark_sum = d->space_sum = 0;
#else
	d->mark_sum = d->space_sum = _FCbuild(0, 0);
#endif
	d->src_offset = 0;

	d->mark_history = malloc(sizeof(*d->mark_history) * d->len);
	d->space_history = malloc(sizeof(*d->mark_history) * d->len);
	memset(d->mark_history, 0, sizeof(*d->mark_history) * d->len);
	memset(d->space_history, 0, sizeof(*d->mark_history) * d->len);
#ifdef AFSK_DEBUG
	if (!debug) debug = fopen("/tmp/afsk.txt", "wb");
#endif

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
afsk_demod(AFSKDemod *const d, void *v_dst, size_t *bit_offset, size_t count, const float *src, size_t len)
{
	uint8_t *dst = v_dst;
	float symbol;
	uint8_t tmp;
	int phase;

	float p_mark = d->p_mark;
	float p_space = d->p_space;
#ifndef _MSC_VER
	float complex out;
	float complex mark_sum = d->mark_sum;
	float complex space_sum = d->space_sum;
#else
	_Fcomplex mark_sum = d->mark_sum;
	_Fcomplex space_sum = d->space_sum;
	_Fcomplex out;
#endif

	if (count < *bit_offset) {
		return PARSED;
	}

	/* Normalize bit offset */
	dst += *bit_offset/8;
	count -= *bit_offset;

	/* Initialize first byte that will be touched */
	tmp = *dst >> (8 - *bit_offset%8);
	d->interm = 0;


	while (count > 0) {
		/* If new read would be out of bounds, ask the reader for more */
		if (d->src_offset >= len) {
			if (*bit_offset%8) *dst = (tmp << (8 - (*bit_offset % 8)));
			d->src_offset = 0;

			/* Save state */
			d->p_mark = p_mark;
			d->p_space = p_space;
			d->mark_sum = mark_sum;
			d->space_sum = space_sum;

			return PROCEED;
		}
		symbol = src[d->src_offset++];
		symbol = agc_apply(&d->agc, symbol) / d->len * 2;

#ifndef _MSC_VER
		/* Calculate mark mix output, update boxcar average over symbol period,
		 * and store the new output in the boxcar history */
		out = symbol * cexpf(-I * p_mark);
		mark_sum += out - d->mark_history[d->idx];
		d->mark_history[d->idx] = out;

		/* Repeat for the space frequency */
		out = symbol * cexpf(-I * p_space);
		space_sum += out - d->space_history[d->idx];
		d->space_history[d->idx] = out;

#else
		/* Thank you, MSVC :( */
		_Fcomplex mark_phasor = _FCbuild(0, -p_mark);
		_Fcomplex space_phasor = _FCbuild(0, -p_space);
		_Fcomplex cplx_symbol = _FCbuild(symbol, 0);

		out = _FCmulcc(cplx_symbol, cexpf(mark_phasor));
		mark_sum = _FCbuild(crealf(mark_sum) + crealf(out) - crealf(d->mark_history[d->idx]),
		                    cimagf(mark_sum) + cimagf(out) - cimagf(d->mark_history[d->idx]));
		d->mark_history[d->idx] = out;

		out = _FCmulcc(cplx_symbol, cexpf(space_phasor));
		space_sum = _FCbuild(crealf(space_sum) + crealf(out) - crealf(d->space_history[d->idx]),
		                    cimagf(space_sum) + cimagf(out) - cimagf(d->space_history[d->idx]));
		d->space_history[d->idx] = out;
#endif

		/* Compute bit output signal */
		symbol = cabsf(mark_sum) - cabsf(space_sum);

		/* Update history buffer index */
		d->idx = (d->idx + 1) % d->len;

		/* Update mark and space phase */
		p_mark = fmod(p_mark + d->f_mark, 2*M_PI);
		p_space = fmod(p_space + d->f_space, 2*M_PI);

		/* Apply filter */
		filter_fwd_sample(&d->lpf, symbol);

#ifdef AFSK_DEBUG
		//fprintf(debug, "%f,%f\n", src[d->src_offset-1], symbol);
#endif

		/* Recover symbol timing */
		for (phase = 0; phase < d->lpf.num_phases; phase++)  {
			switch (advance_timeslot(&d->timing)) {
			case 1:
				/* Half-way slot */
				d->interm = filter_get(&d->lpf, phase);
#ifdef AFSK_DEBUG
				symbol = filter_get(&d->lpf, phase);
				fprintf(debug, "%f,%f\n", symbol, 0.0);
#endif
				break;
			case 2:
				/* Correct slot: update time estimate */
				symbol = filter_get(&d->lpf, phase);
				retime(&d->timing, d->interm, symbol);
#ifdef AFSK_DEBUG
				fprintf(debug, "%f,%f\n", symbol, symbol);
#endif

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
#ifdef AFSK_DEBUG
				symbol = filter_get(&d->lpf, phase);
				fprintf(debug, "%f,%f\n", symbol, 0.0);
#endif
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

	return PARSED;
}
