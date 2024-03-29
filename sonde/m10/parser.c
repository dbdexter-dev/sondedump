#include <float.h>
#include <math.h>
#include <stdio.h>
#include "log/log.h"
#include "parser.h"
#include "gps/time.h"

time_t m10_9f_time(const M10Frame_9f* f) {
	const uint32_t ms = f->time[0] << 24 | f->time[1] << 16 | f->time[2] << 8 | f->time[3];
	const uint16_t week = f->week[0] << 8 | f->week[1];

	return gps_time_to_utc(week, ms);
}
void
m10_9f_serial(char *dst, const M10Frame_9f *frame)
{
	const uint32_t serial_0 = (frame->serial[2] >> 4) * 100 + (frame->serial[2] & 0xF);
	const uint32_t serial_1 = frame->serial[0];
	const uint32_t serial_2 = frame->serial[3] | frame->serial[4] << 8;

	sprintf(dst, "M10-%03d-%d-%1d%04d", serial_0, serial_1, serial_2 >> 13, serial_2 & 0x1FFF);
}

float
m10_9f_lat(const M10Frame_9f* f)
{
	int32_t lat =  f->lat[0] << 24 | f->lat[1] << 16 | f->lat[2] << 8 | f->lat[3];
	return lat * 360.0 / ((uint64_t)1UL << 32);
}

float
m10_9f_lon(const M10Frame_9f* f)
{
	int32_t lon = f->lon[0] << 24 | f->lon[1] << 16 | f->lon[2] << 8 | f->lon[3];
	return lon * 360.0 / ((uint64_t)1UL << 32);
}

float
m10_9f_alt(const M10Frame_9f* f)
{
	int32_t alt =  f->alt[0] << 24 | f->alt[1] << 16 | f->alt[2] << 8 | f->alt[3];
	return alt / 1e3;
}

float
m10_9f_dlat(const M10Frame_9f* f)
{
	int16_t dlat =  f->dlat[0] << 8 | f->dlat[1];
	return dlat / 200.0;
}

float
m10_9f_dlon(const M10Frame_9f* f)
{
	int16_t dlon =  f->dlon[0] << 8 | f->dlon[1];
	return dlon / 200.0;
}

float
m10_9f_dalt(const M10Frame_9f* f)
{
	int16_t dalt = f->dalt[0] << 8 | f->dalt[1];
	return dalt / 200.0;
}

float
m10_9f_temp(const M10Frame_9f* f)
{
	/**
	 * https://www.gruan.org/gruan/editor/documents/meetings/icm-6/pres/pres_306_Haeffelin.pdf
	 * Sensor is a PB5-41E-K1 by Shibaura
	 * B (beta) parameters for the M10 NTC (corrected based on real sounding data)
	 */
	const float ntc_beta = 3100.0f;
	const float ntc_r0 = 15000.0f;
	const float ntc_t0 = 273.15f;
	const float ntc_rinf = ntc_r0 * expf(-ntc_beta / ntc_t0);
	/**
	 * Series/parallel resistors to the NTC, found using
	 * https://youtu.be/a73tuQhQQHE?t=48 and available photos of the m10 board
	 */
	const float ntc_bias[] = {12.1e3, 36.5e3, 475e3};
	const float ntc_parallel[] = {FLT_MAX, 330e3, 2e6};
	/**
	 * MSP430F233T only has 12 bit ADC inputs
	 */
	const uint16_t adc_max = (1 << 12) - 1;
	uint16_t adc_val;
	uint8_t range_idx;
	float adc_percent;
	float resist;
	float temp;

	adc_val = (f->adc_temp_val[0] | f->adc_temp_val[1] << 8) & ((1 << 12) - 1);
	range_idx = MAX(0, f->adc_temp_range);

	adc_percent = adc_val / (float)adc_max;

	/* Compute thermistor resistance given series/parallel pairs */
	switch (range_idx) {
	case 0:
		resist = adc_percent * ntc_bias[0] / (1 - adc_percent);
		break;
	case 1:
	case 2:
		resist = adc_percent * ntc_bias[range_idx] * ntc_parallel[range_idx]
			   / (ntc_parallel[range_idx] - adc_percent * (ntc_bias[range_idx] + ntc_parallel[range_idx]));
		break;
	default:
		/* Invalid range index */
		resist = ntc_rinf;
		break;
	}

	/* Compute temperature given thermistor resistance */
	temp = ntc_beta / logf(resist / ntc_rinf) - 273.15;

	return temp;
}

float
m10_9f_rh(const M10Frame_9f *f)
{
	const float temp_rh_ppm = 400.0e-6f;
	float rh_counts, rh_ref;
	float temp_corr_factor;
	float rh;

	/**
	 * According to the PCB, what they're doing is using the RH capacitor as
	 * part of a 555 oscillator. The output of the oscillator is fed to the
	 * capture input of TIMERB, so that it effectively works as a frequency
	 * counter. They must be doing something else though, because the timer has
	 * a 16 bit rollover, but the transmitted value is 24 bits wide.
	 * Fortunately, it seems they have been kind enough to provide the reference
	 * RH counts at 55% humidity, which can be used to deduce the RH directly
	 * based on the sensor datasheet.
	 */

	rh_counts = f->rh_counts[2] << 16 | f->rh_counts[1] << 8 | f->rh_counts[0];
	rh_ref = f->rh_ref[2] << 16 | f->rh_ref[1] << 8 | f->rh_ref[0];

	/* Apply correction factor as per datasheet */
	temp_corr_factor = 1.0f - temp_rh_ppm * m10_9f_temp(f);

	/* Compute RH% given capacitance (sensor is a GTUS13 or similar) */
	rh = (rh_counts * temp_corr_factor / rh_ref - 0.8955) / 0.002;

	return MAX(0, MIN(100, rh));
}

float
m10_9f_battery(const M10Frame_9f *f)
{
	const float empirical_coeff = 28.2f;    /* TODO figure out actual Vdd and voltage divider ratio */

	float raw_adc_batt = f->adc_batt_val[0] | f->adc_batt_val[1] << 8;

	return raw_adc_batt / (1 << 12) * empirical_coeff;
}

void
m20_20_serial(char *dst, const M20Frame_20 *frame)
{
	const uint32_t raw_serial = frame->sn[2] << 16 | frame->sn[1] << 8 | frame->sn[0];
	const uint8_t serial_0 = raw_serial & 0x3F;
	const uint8_t serial_1 = (raw_serial >> 6) & 0x0F;
	const uint16_t serial_2 = raw_serial >> 10;

	sprintf(dst, "M20-%01d%02d-%d-%05d", serial_0 / 12, serial_0 % 12 + 1, serial_1, serial_2);
}

time_t m20_20_time(const M20Frame_20* f) {
	const uint32_t ms = f->time[0] << 16 | f->time[1] << 8 | f->time[2];
	uint32_t mso = ms;
	mso *= 1000;
	const uint16_t week = f->week[0] << 8 | f->week[1];

	return gps_time_to_utc(week, mso);
}

float m20_20_lat(const M20Frame_20* f) {
	int32_t lat =  f->lat[0] << 24 | f->lat[1] << 16 | f->lat[2] << 8 | f->lat[3];
	return lat / 1e6;
}
float m20_20_lon(const M20Frame_20* f) {
	int32_t lon = f->lon[0] << 24 | f->lon[1] << 16 | f->lon[2] << 8 | f->lon[3];
	return lon / 1e6;
}
float m20_20_alt(const M20Frame_20* f) {
	int32_t alt =  f->alt[0] << 16 | f->alt[1] << 8 | f->alt[2];
	return alt / 1e2;
}
float m20_20_dlat(const M20Frame_20* f) {
	int16_t dlat =  f->dlat[0] << 8 | f->dlat[1];
	return dlat / 100.0;
}
float m20_20_dlon(const M20Frame_20* f) {
	int16_t dlon =  f->dlon[0] << 8 | f->dlon[1];
	return dlon / 100.0;
}
float m20_20_dalt(const M20Frame_20* f) {
	int16_t dalt = f->dalt[0] << 8 | f->dalt[1];
	return dalt / 100.0;
}


float
m20_20_temp(const M20Frame_20 *f)
{
	/**
	 * https://www.gruan.org/gruan/editor/documents/meetings/icm-6/pres/pres_306_Haeffelin.pdf
	 * Sensor is a PB5-41E-K1 by Shibaura
	 * B (beta) parameters for the M10 NTC
	 */
	const float ntc_beta = 3450.0f;
	const float ntc_r0 = 15000.0f;
	const float ntc_t0 = 273.15f;
	const float ntc_rinf = ntc_r0 * expf(-ntc_beta / ntc_t0);
	/**
	 * Series/parallel resistors to the NTC, found using
	 * https://youtu.be/a73tuQhQQHE?t=48 and available photos of the m10 board
	 */
	const float ntc_bias[] = {12.1e3, 36.5e3, 475e3};
	const float ntc_parallel[] = {FLT_MAX, 330e3, 2e6};

	const uint16_t adc_max = (1 << 12) - 1;
	uint16_t raw_val, adc_val;
	uint8_t range_idx;
	float adc_percent;
	float resist;
	float temp;

	/* Extract ADC value and range from raw reading */
	raw_val = (f->adc_temp[0] | f->adc_temp[1] << 8);
	adc_val = raw_val & ((1 << 12) - 1);
	range_idx = raw_val >> 12;

	adc_percent = adc_val / (float)adc_max;

	/* Compute thermistor resistance given series/parallel pairs */
	switch (range_idx) {
	case 0:
		resist = adc_percent * ntc_bias[0] / (1 - adc_percent);
		break;
	case 1:
	case 2:
		resist = adc_percent * ntc_bias[range_idx] * ntc_parallel[range_idx]
			   / (ntc_parallel[range_idx] - adc_percent * (ntc_bias[range_idx] + ntc_parallel[range_idx]));
		break;
	default:
		/* Invalid range index */
		resist = ntc_rinf;
		break;
	}

	/* Compute temperature given thermistor resistance */
	temp = ntc_beta / logf(resist / ntc_rinf) - 273.15;

	return temp;
}
