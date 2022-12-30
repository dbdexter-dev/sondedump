#include "subframe.h"

time_t
ims100_subframe_time(const IMS100FrameGPS *frame)
{
	const uint16_t raw_date = (uint16_t)frame->date[0] << 8 | frame->date[1];
	const uint16_t ms = (uint16_t)frame->ms[0] << 8 | frame->ms[1];
	struct tm tm;
	time_t now;
	int year_unit;

	now = time(NULL);
	tm = *gmtime(&now);

	year_unit = tm.tm_year % 10;
	tm.tm_year -= year_unit;
	tm.tm_year += raw_date % 10 - (year_unit < raw_date % 10 ? 10 : 0);

	tm.tm_mon = (raw_date / 10) % 100 - 1;
	tm.tm_mday = raw_date / 1000;
	tm.tm_hour = frame->hour;
	tm.tm_min = frame->min;
	tm.tm_sec = ms / 1000;

	return my_timegm(&tm);
}

float
ims100_subframe_lat(const IMS100FrameGPS *frame)
{
	const int32_t raw_lat = ((int32_t)frame->lat[0] << 24
	                      | (int32_t)frame->lat[1] << 16
	                      | (int32_t)frame->lat[2] << 8
	                      | (int32_t)frame->lat[3]);
	/* Convert NMEA to decimal degrees */
	float lat = (int)(raw_lat / 1e6) + (raw_lat % 1000000 / 60.0 * 100.0) / 1e6;

	return lat;

}

float
ims100_subframe_lon(const IMS100FrameGPS *frame)
{
	const int32_t raw_lon = ((int32_t)frame->lon[0] << 24
	                      | (int32_t)frame->lon[1] << 16
	                      | (int32_t)frame->lon[2] << 8
	                      | (int32_t)frame->lon[3]);
	/* Convert NMEA to decimal degrees */
	float lon = (int)(raw_lon / 1e6) + (raw_lon % 1000000 / 60.0 * 100.0) / 1e6;

	return lon;
}

float
ims100_subframe_alt(const IMS100FrameGPS *frame)
{
	const int32_t raw_alt = (int32_t)frame->alt[0] << 24
	                      | (int32_t)frame->alt[1] << 16
	                      | (int32_t)frame->alt[2] << 8;

	return (raw_alt >> 8) / 1e2;
}

float
ims100_subframe_speed(const IMS100FrameGPS *frame)
{
	const uint16_t raw_speed = (uint16_t)frame->speed[0] << 8 | (uint16_t)frame->speed[1];

	return raw_speed / 1.943844e2;  // knots*100 -> m/s
}

float
ims100_subframe_heading(const IMS100FrameGPS *frame)
{
	const int16_t raw_heading = (int16_t)frame->heading[0] << 8 | (int16_t)frame->heading[1];

	return abs(raw_heading) / 1e2;
}


