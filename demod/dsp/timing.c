#include <stdio.h>
#include <complex.h>
#include <math.h>
#include "timing.h"
#include "utils.h"

#define FREQ_DEV_EXP 12

static void update_alpha_beta(Timing *t, float damp, float bw);
static void gardner_update_estimate(Timing *t, float err);
static float gardner_err(float prev, float interm, float cur);
static void mm_update_estimate(Timing *t, float err);
static float mm_err(float prev, float cur);

void
timing_init(Timing *t, float sym_freq, float bw)
{
	t->freq = 2*sym_freq;
	t->center_freq = 2*sym_freq;
	t->max_fdev = sym_freq / (1 << FREQ_DEV_EXP);

	t->phase = 0;
	t->state = 1;
	t->prev = 0;

	update_alpha_beta(t, 0.707, bw);
	fprintf(stderr, "%e %e\n", t->alpha, t->beta);
}

int
advance_timeslot(Timing *t)
{
	int ret;

	t->phase += t->freq;

	/* Check if the timeslot is right */
	if (t->phase >= t->state) {
		ret = t->state;
		t->state = (t->state % 2) + 1;
		return ret;
	}

	return 0;
}

void
retime(Timing *t, float interm, float sample)
{
	float err;

	/* Compute timing error */
	err = gardner_err(t->prev, interm, sample);
	t->prev = sample;

	/* Update phase and freq estimate */
	mm_update_estimate(t, err);
}

/* Static functions {{{ */
static float
gardner_err(float prev, float interm, float curr)
{
	return MAX(-2, MIN(2, (curr - prev) * interm));
}

static void
gardner_update_estimate(Timing *t, float error)
{
	t->phase -= 2 - error * t->alpha;
}

static float
mm_err(float prev, float cur)
{
	return sgn(prev)*cur - sgn(cur)*prev;
}

static void
mm_update_estimate(Timing *t, float error)
{
	float freq_delta;

	freq_delta = t->freq - t->center_freq;

	t->phase -= 2 - error * t->alpha;
	freq_delta -= error * t->beta;

	freq_delta = MAX(-t->max_fdev, MIN(t->max_fdev, freq_delta));
	t->freq = t->center_freq + freq_delta;
}


static void
update_alpha_beta(Timing *t, float damp, float bw)
{
	float denom;

	denom = (1 + 2*damp*bw + bw*bw);
	t->alpha = 4*damp*bw/denom;
	t->beta = 4*bw*bw/denom;
}

/* }}} */
