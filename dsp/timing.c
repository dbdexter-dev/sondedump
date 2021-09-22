#include <complex.h>
#include <math.h>
#include "timing.h"
#include "utils.h"

/* freq will be at most +-2**-FREQ_DEV_EXP outside of the range */
#define FREQ_DEV_EXP 12

static void update_estimate(Timing *t, float err);
static void update_alpha_beta(Timing *t, float damp, float bw);
static float mm_err(float prev, float cur);
static float gardner_err(float prev, float interm, float cur);


void
timing_init(Timing *t, float sym_freq, float alpha)
{
	t->freq = 2.0*M_PI*sym_freq;
	t->alpha = alpha;
	t->phase = 0;
	t->state = 1;
}

int
advance_timeslot(Timing *t)
{
	int ret;

	t->phase += t->freq;

	/* Update the intermediate sample */
	if (t->phase >= t->state * M_PI) {
		ret = t->state;
		t->state = (t->state % 2) + 1;
		return ret;
	}

	/* Check if the timeslot is right */
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
	update_estimate(t, err);
}

/* Static functions {{{ */
static void
update_estimate(Timing *t, float error)
{
	t->phase -= 2*M_PI - error * t->alpha;
}

static float
gardner_err(float prev, float interm, float curr)
{
	return (curr - prev) * interm;
}

/* }}} */
