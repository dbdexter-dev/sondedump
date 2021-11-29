#include <math.h>
#include <stdlib.h>
#include "agc.h"
#include "utils.h"

#define FLOAT_TARGET_MAG 200
#define BIAS_POLE 0.01f
#define GAIN_POLE 0.001f

void
agc_init(Agc *agc)
{
	agc->gain = 1;
	agc->bias = 0;
	agc->moving_avg = FLOAT_TARGET_MAG;
}

float
agc_apply(Agc *agc, float sample)
{
	if (sample == 0) return 0;
	sample -= agc->bias;
	agc->bias = agc->bias * (1-BIAS_POLE) + sample*BIAS_POLE;

	agc->gain = FLOAT_TARGET_MAG/agc->moving_avg;
	agc->moving_avg = agc->moving_avg * (1-GAIN_POLE) + fabsf(sample)*GAIN_POLE;     /* Prevents div/0 above */
	/* Apply AGC */
	sample *= agc->gain;

	return sample;
}
