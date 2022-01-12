#include <math.h>
#include "bitops.h"
#include "protocol.h"
#include "subframe.h"
#include "utils.h"

float
dfm09_subframe_temp(const DFM09Subframe_PTU *ptu, const DFM09Calib *calib)
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

uint32_t
dfm09_subframe_seq(const DFM09Subframe_GPS *gps)
{
	return bitmerge(gps->data + 3, 8);
}

int
dfm09_subframe_time(const DFM09Subframe_GPS *gps)
{
	return bitmerge(gps->data + 4, 16) / 1000;
}

void
dfm09_subframe_date(struct tm *dst, const DFM09Subframe_GPS *gps)
{
	const uint32_t raw = bitmerge(gps->data, 32);

	dst->tm_year = ((raw >> (32 - 12)) & 0xFFF) - 1900;
	dst->tm_mon = ((raw >> (32 - 16)) & 0xF) - 1;
	dst->tm_mday = (raw >> (32 - 21)) & 0x1F;
	dst->tm_hour = (raw >> (32 - 26)) & 0x1F;
	dst->tm_min = raw & 0x3F;
}

float
dfm09_subframe_lat(const DFM09Subframe_GPS *gps)
{
	return (int32_t)bitmerge(gps->data, 32) / 1e7;
}

float
dfm09_subframe_lon(const DFM09Subframe_GPS *gps)
{
	return (int32_t)bitmerge(gps->data, 32) / 1e7;
}
float
dfm09_subframe_alt(const DFM09Subframe_GPS *gps)
{
	return (int32_t)bitmerge(gps->data, 32) / 1e2;
}

float
dfm09_subframe_spd(const DFM09Subframe_GPS *gps)
{
	return bitmerge(gps->data + 4, 16) / 1e2;
}

float
dfm09_subframe_hdg(const DFM09Subframe_GPS *gps)
{
	return bitmerge(gps->data + 4, 16) / 1e2;
}

float
dfm09_subframe_climb(const DFM09Subframe_GPS *gps)
{
	return (int16_t)bitmerge(gps->data + 4, 16) / 1e2;
}
