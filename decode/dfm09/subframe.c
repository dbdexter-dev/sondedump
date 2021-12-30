#include <math.h>
#include "subframe.h"
#include "protocol.h"
#include "utils.h"

float
dfm09_subframe_temp(DFM09Subframe_PTU *ptu, DFM09Calib *calib)
{
	const uint32_t raw_temp = bitmerge(ptu->data, 24);
	const uint32_t raw_ref1 = calib->raw[3];
	const uint32_t raw_ref2 = calib->raw[4];

	const float f_raw_temp = (float)(raw_temp & 0xFFFFF) / (1 << (raw_temp >> 20));
	const float f_raw_ref1 = (float)(raw_ref1 & 0xFFFFF) / (1 << (raw_ref1 >> 20));
	const float f_raw_ref2 = (float)(raw_ref2 & 0xFFFFF) / (1 << (raw_ref2 >> 20));

	const float bb0 = 3260.0;
	const float t0 = 25 + 273.15;
	const float r0 = 5.0e3;
	const float rf = 220e3;

	float g = f_raw_ref2 / rf;
	float r = (f_raw_temp - f_raw_ref1) / g;
	float temp = 0;

	if (!raw_temp || !raw_ref1 || !raw_ref2) r = 0;
	if (r > 0) temp = 1.0 / (1/t0 + 1/bb0 * logf(r/r0)) - 273.15;

	return temp;
}
