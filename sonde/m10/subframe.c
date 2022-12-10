#include <float.h>
#include <math.h>
#include <stdio.h>
#include "log/log.h"
#include "subframe.h"
#include "gps/time.h"

time_t m10_frame_9f_time(const M10Frame_9f* f) {
	const uint32_t ms = f->time[0] << 24 | f->time[1] << 16 | f->time[2] << 8 | f->time[3];
	const uint16_t week = f->week[0] << 8 | f->week[1];

	return gps_time_to_utc(week, ms);
}
void
m10_frame_9f_serial(char *dst, const M10Frame_9f *frame)
{
	sprintf(dst, "ME%02X%01X%02X%02X", frame->data[4], frame->data[2], frame->data[6], frame->data[5]);
}

float
m10_frame_9f_lat(const M10Frame_9f* f)
{
	int32_t lat =  f->lat[0] << 24 | f->lat[1] << 16 | f->lat[2] << 8 | f->lat[3];
	return lat * 360.0 / ((uint64_t)1UL << 32);
}

float
m10_frame_9f_lon(const M10Frame_9f* f)
{
	int32_t lon = f->lon[0] << 24 | f->lon[1] << 16 | f->lon[2] << 8 | f->lon[3];
	return lon * 360.0 / ((uint64_t)1UL << 32);
}

float
m10_frame_9f_alt(const M10Frame_9f* f)
{
	int32_t alt =  f->alt[0] << 24 | f->alt[1] << 16 | f->alt[2] << 8 | f->alt[3];
	return alt / 1e3;
}

float
m10_frame_9f_dlat(const M10Frame_9f* f)
{
	int16_t dlat =  f->dlat[0] << 8 | f->dlat[1];
	return dlat / 200.0;
}

float
m10_frame_9f_dlon(const M10Frame_9f* f)
{
	int16_t dlon =  f->dlon[0] << 8 | f->dlon[1];
	return dlon / 200.0;
}

float
m10_frame_9f_dalt(const M10Frame_9f* f)
{
	int16_t dalt = f->dalt[0] << 8 | f->dalt[1];
	return dalt / 200.0;
}

float
m10_frame_9f_temp(const M10Frame_9f* f)
{
	/* https://www.gruan.org/gruan/editor/documents/meetings/icm-6/pres/pres_306_Haeffelin.pdf
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
	if (range_idx == 0) {
		resist = adc_percent * ntc_bias[0] / (1 - adc_percent);
	} else {
		resist = adc_percent * ntc_bias[range_idx] * ntc_parallel[range_idx]
			   / (ntc_parallel[range_idx] - adc_percent * (ntc_bias[range_idx] + ntc_parallel[range_idx]));
	}

	/* Compute temperature given thermistor resistance */
	temp = ntc_beta / logf(resist / ntc_rinf) - 273.15;

	return temp;
}

float
m10_frame_9f_rh(const M10Frame_9f* f)
{
	(void)f;
	return 0;
}

void
m20_frame_20_serial(char *dst, const M20Frame_20 *frame)
{
    uint32_t id = frame->sn[0];
    uint16_t idn = ((frame->sn[2]<<8)|frame->sn[1])/4;
	sprintf(dst, "ME%01X%01X%01X%01X%01X%01X%01X", (id/16)&0xF, id&0xF, (idn/10000)%10, (idn/1000)%10, (idn/100)%10, (idn/10)%10, idn%10);
}

time_t m20_frame_20_time(const M20Frame_20* f) {
	const uint32_t ms = f->time[0] << 16 | f->time[1] << 8 | f->time[2];
	uint32_t mso = ms;
	mso *= 1000;
	const uint16_t week = f->week[0] << 8 | f->week[1];

	return gps_time_to_utc(week, mso);
}

float m20_frame_20_lat(const M20Frame_20* f) {
	int32_t lat =  f->lat[0] << 24 | f->lat[1] << 16 | f->lat[2] << 8 | f->lat[3];
	return lat / 1e6;
}
float m20_frame_20_lon(const M20Frame_20* f) {
	int32_t lon = f->lon[0] << 24 | f->lon[1] << 16 | f->lon[2] << 8 | f->lon[3];
	return lon / 1e6;
}
float m20_frame_20_alt(const M20Frame_20* f) {
	int32_t alt =  f->alt[0] << 16 | f->alt[1] << 8 | f->alt[2];
	return alt / 1e2;
}
float m20_frame_20_dlat(const M20Frame_20* f) {
	int16_t dlat =  f->dlat[0] << 8 | f->dlat[1];
	return dlat / 100.0;
}
float m20_frame_20_dlon(const M20Frame_20* f) {
	int16_t dlon =  f->dlon[0] << 8 | f->dlon[1];
	return dlon / 100.0;
}
float m20_frame_20_dalt(const M20Frame_20* f) {
	int16_t dalt = f->dalt[0] << 8 | f->dalt[1];
	return dalt / 100.0;
}
