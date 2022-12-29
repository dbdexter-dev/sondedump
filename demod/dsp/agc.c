#include <math.h>
#include <stdlib.h>
#include "agc.h"
#include "utils.h"
#include "log/log.h"

#define FLOAT_TARGET_MAG 5
#define BIAS_POLE 0.01f
#define GAIN_POLE 0.001f

void
agc_init(Agc *agc)
{
	agc->moving_avg = FLOAT_TARGET_MAG;
}

float
agc_apply(Agc *agc, float sample)
{
	float gain;

	/* Prevent zero samples from saturating the gain */
	if (sample == 0) return 0;

	gain = FLOAT_TARGET_MAG/agc->moving_avg;
	agc->moving_avg = agc->moving_avg * (1-GAIN_POLE) + fabsf(sample)*GAIN_POLE;     /* Prevents div/0 above */

	/* Apply AGC */
	sample *= gain;

	return sample;
}
