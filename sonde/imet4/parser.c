#include <math.h>
#include <time.h>
#include "utils.h"
#include "parser.h"

float
imet4_ptu_temp(const IMET4Subframe_PTU *ptu)
{
	return ptu->temp / 100.0f;
}
float
imet4_ptu_rh(const IMET4Subframe_PTU *ptu)
{
	return ptu->rh / 100.0f;
}
float
imet4_ptu_pressure(const IMET4Subframe_PTU *ptu)
{
	uint32_t raw = ptu->pressure[0] | ptu->pressure[1] << 8 | ptu->pressure[2] << 16;

	return raw / 100.0f;
}

float
imet4_gps_lat(const IMET4Subframe_GPS *gps)
{
	return gps->lat;
}
float
imet4_gps_lon(const IMET4Subframe_GPS *gps)
{
	return gps->lon;
}
float
imet4_gps_alt(const IMET4Subframe_GPS *gps)
{
	return gps->alt - 5000.f;
}

time_t
imet4_gps_time(const IMET4Subframe_GPS *gps)
{
	time_t now;
	struct tm datetime;

	/* Date is not transmitted: use current date */
	now = time(NULL);
	datetime = *gmtime(&now);

	/* Handle 0Z crossing */
	if (abs(gps->hour - datetime.tm_hour) >= 12) {
		now += (gps->hour < datetime.tm_hour) ? 86400 : -86400;
		datetime = *gmtime(&now);
	}

	datetime.tm_hour = gps->hour;
	datetime.tm_min = gps->min;
	datetime.tm_sec = gps->sec;

	return my_timegm(&datetime);
}

time_t
imet4_gpsx_time(const IMET4Subframe_GPSX *gps)
{
	time_t now;
	struct tm datetime;

	/* Date is not transmitted: use current date */
	now = time(NULL);
	datetime = *gmtime(&now);

	/* Handle 0Z crossing */
	if (abs(gps->hour - datetime.tm_hour) >= 12) {
		now += (gps->hour < datetime.tm_hour) ? 86400 : -86400;
		datetime = *gmtime(&now);
	}

	datetime.tm_hour = gps->hour;
	datetime.tm_min = gps->min;
	datetime.tm_sec = gps->sec;

	return my_timegm(&datetime);
}

float
imet4_gpsx_speed(const IMET4Subframe_GPSX *gps)
{
	return sqrtf(gps->dlat * gps->dlat + gps->dlon * gps->dlon);
}

float
imet4_gpsx_heading(const IMET4Subframe_GPSX *gps)
{
	float ret;

	ret = atan2f(gps->dlat, gps->dlon) / 180.0f / M_PI;
	if (ret < 0) ret += 360.0;

	return ret;
}

float
imet4_gpsx_climb(const IMET4Subframe_GPSX *gps)
{
	return gps->climb;
}
