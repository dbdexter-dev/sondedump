#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "decode/ecc/crc.h"
#include "subframe.h"
#include "utils.h"

static float wv_sat_pressure(float temp);

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

	float adc_raw, r_raw, r_t, t_uncal, t_cal;
	int i;

	/* If no reference or no calibration data, retern */
	if (adc_ref2 - adc_ref1 == 0) return NAN;

	/* Compute ADC gain and bias */
	adc_raw = (adc_main - adc_ref1) / (adc_ref2 - adc_ref1);

	/* Compute resistance */
	r_raw = calib->t_ref[0] + (calib->t_ref[1] - calib->t_ref[0])*adc_raw;
	r_t = r_raw * calib->t_calib_coeff[0];

	/* Compute temperature based on corrected resistance */
	t_uncal = calib->t_temp_poly[0]
	     + calib->t_temp_poly[1]*r_t
	     + calib->t_temp_poly[2]*r_t*r_t;

	t_cal = 0;
	for (i=6; i>0; i--) {
		t_cal *= t_uncal;
		t_cal += calib->t_calib_coeff[i];
	}
	t_cal += t_uncal;

	return t_cal;
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

	int i, j;
	float f1, f2;
	float adc_raw, c_raw, c_cal, rh_uncal, rh_cal, rh_temp_uncal, rh_temp, t_temp;

	if (adc_ref2 - adc_ref1 == 0) return NAN;

	/* Get RH sensor temperature and actual temperature */
	rh_temp_uncal = rs41_subframe_temp_humidity(ptu, calib);
	t_temp = rs41_subframe_temp(ptu, calib);

	/* Compute RH calibrated temperature */
	rh_temp = 0;
	for (i=6; i>0; i--) {
		rh_temp *= rh_temp_uncal;
		rh_temp += calib->th_calib_coeff[i];
	}
	rh_temp += rh_temp_uncal;

	/* Get raw capacitance of the RH sensor */
	adc_raw = (adc_main - adc_ref1) / (adc_ref2 - adc_ref1);
	c_raw = calib->rh_ref[0] + adc_raw * (calib->rh_ref[1] - calib->rh_ref[0]);
	c_cal = (c_raw / calib->rh_cap_calib[0] - 1) * calib->rh_cap_calib[1];

	/* Derive raw RH% from capacitance and temperature response */
	rh_uncal = 0;
	rh_temp = (rh_temp - 20) / 180;
	f1 = 1;
	for (i=0; i<7; i++) {
		f2 = 1;
		for (j=0; j<6; j++) {
			rh_uncal += f1 * f2 * calib->rh_calib_coeff[i][j];
			f2 *= rh_temp;
		}
		f1 *= c_cal;
	}

	/* Account for different temperature between air and RH sensor */
	rh_cal = rh_uncal * wv_sat_pressure(rh_temp_uncal) / wv_sat_pressure(t_temp);
	return MAX(0, MIN(100, rh_cal));
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

	float adc_raw, r_raw, r_t, t_uncal;

	/* If no reference or no calibration data, retern */
	if (adc_ref2 - adc_ref1 == 0) return NAN;
	if (!calib->t_ref[0] || !calib->t_ref[1]) return NAN;

	/* Compute ADC gain and bias */
	adc_raw = (adc_main - adc_ref1) / (adc_ref2 - adc_ref1);

	/* Compute resistance */
	r_raw = calib->t_ref[0] + adc_raw * (calib->t_ref[1] - calib->t_ref[0]);
	r_t = r_raw * calib->th_calib_coeff[0];

	/* Compute temperature based on corrected resistance */
	t_uncal = calib->th_temp_poly[0]
	     + calib->th_temp_poly[1]*r_t
	     + calib->th_temp_poly[2]*r_t*r_t;

	return t_uncal;
}

float
rs41_subframe_pressure(RS41Subframe_PTU *ptu, RS41Calibration *calib)
{
	const float adc_main = (uint32_t)ptu->pressure_main[0]
	                      | (uint32_t)ptu->pressure_main[1] << 8
	                      | (uint32_t)ptu->pressure_main[2] << 16;
	const float adc_ref1 = (uint32_t)ptu->pressure_ref1[0]
	                      | (uint32_t)ptu->pressure_ref1[1] << 8
	                      | (uint32_t)ptu->pressure_ref1[2] << 16;
	const float adc_ref2 = (uint32_t)ptu->pressure_ref2[0]
	                      | (uint32_t)ptu->pressure_ref2[1] << 8
	                      | (uint32_t)ptu->pressure_ref2[2] << 16;

	const float pt = ptu->pressure_temp;
	float adc_raw, pressure, p_poly[6];

	/* If no reference, return */
	if (adc_ref2 - adc_ref1 == 0) return NAN;
	if (adc_main == 0) return NAN;

	/* Compute ADC raw value */
	adc_raw = (adc_main - adc_ref1) / (adc_ref2 - adc_ref1);

	/* Derive pressure from raw value */
	p_poly[0] = calib->p_calib_coeff[0]
	          + calib->p_calib_coeff[7] * pt
	          + calib->p_calib_coeff[11] * pt * pt
	          + calib->p_calib_coeff[15] * pt * pt * pt;
	p_poly[1] = calib->p_calib_coeff[1]
	          + calib->p_calib_coeff[8] * pt
	          + calib->p_calib_coeff[12] * pt * pt
	          + calib->p_calib_coeff[16] * pt * pt * pt;
	p_poly[2] = calib->p_calib_coeff[2]
	          + calib->p_calib_coeff[9] * pt
	          + calib->p_calib_coeff[13] * pt * pt
	          + calib->p_calib_coeff[17] * pt * pt * pt;
	p_poly[3] = calib->p_calib_coeff[3]
	          + calib->p_calib_coeff[10] * pt
	          + calib->p_calib_coeff[14] * pt * pt;
	p_poly[4] = calib->p_calib_coeff[4];
	p_poly[5] = calib->p_calib_coeff[5];

	pressure = p_poly[0]
	         + p_poly[1] * adc_raw
	         + p_poly[2] * adc_raw * adc_raw
	         + p_poly[3] * adc_raw * adc_raw * adc_raw
	         + p_poly[4] * adc_raw * adc_raw * adc_raw * adc_raw
	         + p_poly[5] * adc_raw * adc_raw * adc_raw * adc_raw * adc_raw;


	return pressure;
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

static float
wv_sat_pressure(float temp)
{
	const float coeffs[] = {-0.493158, 1 + 4.6094296e-3, -1.3746454e-5, 1.2743214e-8};
	float T, p;
	int i;

	temp += 273.15f;

	T = 0;
	for (i=LEN(coeffs)-1; i>=0; i--) {
		T *= temp;
		T += coeffs[i];
	}

	p = expf(-5800.2206f / T
		  + 1.3914993f
		  + 6.5459673f * logf(T)
		  - 4.8640239e-2f * T
		  + 4.1764768e-5f * T * T
		  - 1.4452093e-8f * T * T * T);

	return p / 100.0f;

}

