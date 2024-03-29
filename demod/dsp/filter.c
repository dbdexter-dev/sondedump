#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "filter.h"
#include "utils.h"

static float rc_coeff(float cutoff, int stage_no, unsigned num_taps, float osf, float alpha);

int
filter_init_lpf(Filter *flt, int order, float cutoff, int num_phases)
{
	int i, phase;
	const int taps = order * 2 + 1;

	if (!(flt->coeffs = malloc(num_phases * sizeof(*flt->coeffs) * taps))) return 1;
	if (!(flt->mem = calloc(2 * taps, sizeof(*flt->mem)))) return 1;

	cutoff /= num_phases;

	for (phase = 0; phase < num_phases; phase++) {
		for (i=0; i<taps; i++) {
			flt->coeffs[phase * taps + i] = rc_coeff(cutoff, i * num_phases + phase, taps * num_phases, num_phases, 0.99);
		}
	}

	flt->num_phases = num_phases;
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
	/* Store two copies to make output computation faster */
	flt->mem[flt->idx] = flt->mem[flt->idx + flt->size] = sample;
	flt->idx = (flt->idx + 1) % flt->size;
}

float
filter_get(const Filter *const flt, int phase)
{
	int i, j;
	float result;

	j = flt->size * (flt->num_phases - phase - 1);
	result = 0;

	/* No need to wrap around because of the second copy of all samples stored
	 * starting from flt->size */
	for (i=flt->idx; i<flt->size + flt->idx; i++, j++) {
		result += flt->mem[i] * flt->coeffs[j];
	}

	return result;
}

static float
rc_coeff(float cutoff, int stage_no, unsigned taps, float osf, float alpha)
{
	float rc_coeff, hamming_coeff;
	float t;
	const int order = (taps - 1) / 2;
	const float norm = 2.0/5.0 * order / osf / 10.0;


	t = abs(order - stage_no) / osf;

	if (t == 0) {
		rc_coeff = cutoff;
	} else if (2*alpha*t*cutoff == 1) {
		rc_coeff = -M_PI/(4*cutoff) * sinf(M_PI/(2*alpha))/(M_PI/(2*alpha));
	} else {
		/* Raised cosine coefficient */
		rc_coeff = sinf(M_PI*t*cutoff)/(M_PI*t)
		         * cosf(M_PI*alpha*t*cutoff) / (1 - powf(2*alpha*t*cutoff, 2));
	}

	/* Hamming windowing function */
	hamming_coeff = 0.42
		- 0.5*cosf(2*M_PI*stage_no/(taps-1))
		+ 0.08*cosf(4*M_PI*stage_no/(taps-1));

	return norm * rc_coeff * hamming_coeff;
}
