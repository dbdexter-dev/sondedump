#include <math.h>
#include <stdlib.h>
#include "agc.h"
#include "utils.h"

#define FLOAT_TARGET_MAG 10000
#define BIAS_POLE 0.001f
#define GAIN_POLE 0.01f

static float _moving_avg = FLOAT_TARGET_MAG;
static float _float_gain = 1;
static float _float_bias = 0;

float
agc_apply(float sample)
{
	sample -= _float_bias;
	_float_bias = _float_bias * (1-BIAS_POLE) + sample*BIAS_POLE;

	_float_gain = MAX(0, FLOAT_TARGET_MAG/_moving_avg);
	_moving_avg = _moving_avg * (1-GAIN_POLE) + fabsf(sample)*GAIN_POLE + 1e-9;     /* Prevents div/0 above */
	/* Apply AGC */
	sample *= _float_gain;

	return sample;
}

float
agc_get_gain()
{
	return _float_gain;
}
