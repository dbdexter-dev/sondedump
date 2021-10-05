#include <complex.h>
#include <math.h>
#include "timing.h"
#include "utils.h"

static void update_estimate(Timing *t, float err);
static float gardner_err(float prev, float interm, float cur);

void
timing_init(Timing *t, float sym_freq, float alpha)
{
	t->freq = 2.0*sym_freq;
	t->alpha = alpha;
	t->phase = 0;
	t->state = 1;
	t->prev = 0;
}

int
advance_timeslot(Timing *t)
{
	int ret;

	t->phase += t->freq;

	/* Update the intermediate sample */
	if (t->phase >= t->state) {
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
	t->phase -= 2 - error * t->alpha;
}

static float
gardner_err(float prev, float interm, float curr)
{
	return (curr - prev) * interm;
}

/* }}} */
