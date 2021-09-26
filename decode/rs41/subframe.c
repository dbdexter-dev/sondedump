#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ecc/crc.h"
#include "subframe.h"
#include "utils.h"

float
rs41_subframe_temp(RS41Subframe_PTU *ptu, RS41Calibration *calib)
{
	const float adc_main = (uint32_t)ptu->temp_main[0]
	                      | (uint32_t)ptu->temp_main[1] << 8
	                      | (uint32_t)ptu->temp_main[2] << 16;
	const float adc_ref1 = (uint32_t)ptu->temp_ref1[0]
	                      | (uint32_t)ptu->temp_ref1[1] << 8
	                      | (uint32_t)ptu->temp_ref1[2] << 16;
	const float adc_ref2 = (uint32_t)ptu->temp_ref2[0]
	                      | (uint32_t)ptu->temp_ref2[1] << 8
	                      | (uint32_t)ptu->temp_ref2[2] << 16;

	float adc_gain, adc_bias, r_raw, r_t, temp;

	/* If no reference or no calibration data, retern */
	if (adc_ref2 - adc_ref1 == 0) return -1;
	if (!calib->rt_ref[0] || !calib->rt_ref[1]) return -1;

	/* Compute ADC gain and bias */
	adc_gain = ((adc_ref2 - adc_ref1) / (calib->rt_ref[1] - calib->rt_ref[0]));
	adc_bias = adc_ref2 - adc_gain*calib->rt_ref[1];

	/* Compute resistance */
	r_raw = (adc_main - adc_bias) / adc_gain;
	r_t = r_raw * calib->rt_resist_coeff[0];

	/* Compute temperature based on corrected resistance */
	temp = calib->rt_temp_poly[0]
	     + calib->rt_temp_poly[1]*r_t
	     + calib->rt_temp_poly[2]*r_t*r_t;

	return calib->rt_resist_coeff[1] + temp*(1+calib->rt_resist_coeff[2]);
}

float
rs41_subframe_humidity(RS41Subframe_PTU *ptu, RS41Calibration *calib)
{
	float adc_main = (uint32_t)ptu->humidity_main[0]
	                       | (uint32_t)ptu->humidity_main[1] << 8
	                       | (uint32_t)ptu->humidity_main[2] << 16;
	float adc_ref1 = (uint32_t)ptu->humidity_ref1[0]
	                       | (uint32_t)ptu->humidity_ref1[1] << 8
	                       | (uint32_t)ptu->humidity_ref1[2] << 16;
	float adc_ref2 = (uint32_t)ptu->humidity_ref2[0]
	                       | (uint32_t)ptu->humidity_ref2[1] << 8
	                       | (uint32_t)ptu->humidity_ref2[2] << 16;

	float rh_measure, rh, temp;

	if (adc_ref2 - adc_ref1 == 0) return -1;


	temp = rs41_subframe_temp_humidity(ptu, calib);
	//printf("%f %f %f %f ", adc_main, adc_ref1, adc_ref2, temp);

	rh_measure = (adc_main - adc_ref1) / (adc_ref2 - adc_ref1);
	rh = 100 * (350/calib->rh_cap_coeff[0]*rh_measure - 7.5);
	rh += -temp/5.5;
	if (temp < -25) rh *= 1 -25/90;


	/* Temperature compensation */
	rh += rh*calib->rh_cap_coeff[1]/100;

	return MAX(0, MIN(100, rh));
}

float
rs41_subframe_temp_humidity(RS41Subframe_PTU *ptu, RS41Calibration *calib)
{
	const float adc_main = (uint32_t)ptu->temp_humidity_main[0]
	                      | (uint32_t)ptu->temp_humidity_main[1] << 8
	                      | (uint32_t)ptu->temp_humidity_main[2] << 16;
	const float adc_ref1 = (uint32_t)ptu->temp_humidity_ref1[0]
	                      | (uint32_t)ptu->temp_humidity_ref1[1] << 8
	                      | (uint32_t)ptu->temp_humidity_ref1[2] << 16;
	const float adc_ref2 = (uint32_t)ptu->temp_humidity_ref2[0]
	                      | (uint32_t)ptu->temp_humidity_ref2[1] << 8
	                      | (uint32_t)ptu->temp_humidity_ref2[2] << 16;

	float adc_gain, adc_bias, r_raw, r_t, temp;

	/* If no reference or no calibration data, retern */
	if (adc_ref2 - adc_ref1 == 0) return -1;
	if (!calib->rt_ref[0] || !calib->rt_ref[1]) return -1;


	/* Compute ADC gain and bias */
	adc_gain = ((adc_ref2 - adc_ref1) / (calib->rt_ref[1] - calib->rt_ref[0]));
	adc_bias = adc_ref2 - adc_gain*calib->rt_ref[1];

	/* Compute resistance */
	r_raw = (adc_main - adc_bias) / adc_gain;
	r_t = r_raw * calib->rh_resist_coeff[0];

	/* Compute temperature based on corrected resistance */
	temp = calib->rh_temp_poly[0]
	     + calib->rh_temp_poly[1]*r_t
	     + calib->rh_temp_poly[2]*r_t*r_t;

	return calib->rh_resist_coeff[1] + temp*(1+calib->rh_resist_coeff[2]);
}

float
rs41_subframe_pressure(RS41Subframe_PTU *ptu, RS41Calibration *calib)
{
	uint32_t pressure_main = (uint32_t)ptu->pressure_main[0]
	                       | (uint32_t)ptu->pressure_main[1] << 8
	                       | (uint32_t)ptu->pressure_main[2] << 16;
	uint32_t pressure_ref1 = (uint32_t)ptu->pressure_ref1[0]
	                       | (uint32_t)ptu->pressure_ref1[1] << 8
	                       | (uint32_t)ptu->pressure_ref1[2] << 16;
	uint32_t pressure_ref2 = (uint32_t)ptu->pressure_ref2[0]
	                       | (uint32_t)ptu->pressure_ref2[1] << 8
	                       | (uint32_t)ptu->pressure_ref2[2] << 16;

	if (pressure_ref2 - pressure_ref1 == 0) return -1;

	float percent = ((float)pressure_main - pressure_ref1) / (pressure_ref2 - pressure_ref1);
	return -1;
}

float
rs41_subframe_temp_pressure(RS41Subframe_PTU *ptu, RS41Calibration *calib)
{
	return ptu->pressure_temp/100.0;
}

float
rs41_subframe_x(RS41Subframe_GPSPos *gps)
{
	return gps->x / 100.0;
}

float
rs41_subframe_y(RS41Subframe_GPSPos *gps)
{
	return gps->y / 100.0;
}

float
rs41_subframe_z(RS41Subframe_GPSPos *gps)
{
	return gps->z / 100.0;
}

float
rs41_subframe_dx(RS41Subframe_GPSPos *gps)
{
	return gps->dx / 100.0;
}
float
rs41_subframe_dy(RS41Subframe_GPSPos *gps)
{
	return gps->dy / 100.0;
}
float
rs41_subframe_dz(RS41Subframe_GPSPos *gps)
{
	return gps->dz / 100.0;
}
