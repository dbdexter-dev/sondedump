#include <math.h>
#include <stdlib.h>
#include "agc.h"
#include "utils.h"

#define FLOAT_TARGET_MAG 1024
#define BIAS_POLE 0.01f
#define GAIN_POLE 0.001f

static float _float_gain = 1;
static float _float_bias = 0;

float
agc_apply(float sample)
{
	/* Remove DC bias */
	_float_bias = _float_bias*(1-BIAS_POLE) + BIAS_POLE*sample;
	sample -= _float_bias;

	/* Apply AGC */
	sample *= _float_gain;
	_float_gain += GAIN_POLE*(FLOAT_TARGET_MAG - fabs(sample));
	_float_gain = MAX(0, _float_gain);

	return sample;
}

float
agc_get_gain()
{
	return _float_gain;
}
