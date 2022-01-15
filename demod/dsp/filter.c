#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "filter.h"
#include "utils.h"

static float sinc_coeff(float cutoff, int stage_no, unsigned num_taps, float osf);
static float rc_coeff(float cutoff, int stage_no, unsigned num_taps, float osf, float alpha);

int
filter_init_lpf(Filter *flt, int order, float cutoff)
{
	int i;
	const int taps = order * 2 + 1;

	if (!(flt->coeffs = malloc(sizeof(*flt->coeffs) * taps))) return 1;
	if (!(flt->mem = calloc(taps, sizeof(*flt->mem)))) return 1;

	for (i=0; i<taps; i++) {
		flt->coeffs[i] = rc_coeff(cutoff, i, taps, 1, 0.4);
	}

	flt->size = taps;
	flt->idx = 0;

	return 0;
}

void
filter_deinit(Filter *flt)
{
	free(flt->coeffs);
	free(flt->mem);
}

void
filter_fwd_sample(Filter *flt, float sample)
{
	flt->mem[flt->idx++] = sample;
	flt->idx %= flt->size;
}

float
filter_get(Filter *flt)
{
	int i, j;
	float result;

	j = 0;
	result = 0;

	/* Chunk 1: from current position to end */
	for (i=flt->idx; i<flt->size; i++, j++) {
		result += flt->mem[i] * flt->coeffs[j];
	}

	/* Chunk 2: from start to current position - 1 */
	for (i=0; i<flt->idx; i++, j++) {
		result += flt->mem[i] * flt->coeffs[j];
	}

	return result;
}

static float
sinc_coeff(float cutoff, int stage_no, unsigned taps, float osf)
{
	const float norm = 2.0/5.0;
	float sinc_coeff, hamming_coeff;
	float t;
	int order;

	order = (taps - 1) / 2;

	if (order == stage_no) {
		return norm;
	}

	t = abs(order - stage_no) / osf;

	/* Sinc coefficient */
	sinc_coeff = sinf(2*M_PI*t*cutoff)/(2*M_PI*t*cutoff);

	/* Hamming windowing function */
	hamming_coeff = 0.42
		- 0.5*cosf(2*M_PI*stage_no/(taps-1))
		+ 0.08*cosf(4*M_PI*stage_no/(taps-1));

	return norm * sinc_coeff * hamming_coeff;
}

static float
rc_coeff(float cutoff, int stage_no, unsigned taps, float osf, float alpha)
{
	const float norm = 2.0/5.0;
	float rc_coeff, hamming_coeff;
	float t;
	int order;

	order = (taps - 1) / 2;

	if (order == stage_no) {
		return norm;
	}

	t = abs(order - stage_no) / osf;

	/* Raised cosine coefficient */
	rc_coeff = sinf(2*M_PI*t*cutoff)/(2*M_PI*t*cutoff)
	         * cosf(M_PI*alpha*t) / (1 - powf(2*alpha*t, 2));


	/* Hamming windowing function */
	hamming_coeff = 0.42
		- 0.5*cosf(2*M_PI*stage_no/(taps-1))
		+ 0.08*cosf(4*M_PI*stage_no/(taps-1));

	return norm * rc_coeff * hamming_coeff;
}
