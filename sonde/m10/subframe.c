#include <stdio.h>
#include "subframe.h"
#include "gps/time.h"

time_t m10_frame_9f_time(M10Frame_9f* f) {
	const uint32_t ms = f->time[0] << 24 | f->time[1] << 16 | f->time[2] << 8 | f->time[3];
	const uint16_t week = f->week[0] << 8 | f->week[1];

	return gps_time_to_utc(week, ms);
}
void
m10_frame_9f_serial(char *dst, M10Frame_9f *frame)
{
	sprintf(dst, "ME%02X%02X%02X%02X", frame->data[59], frame->data[60], frame->data[61], frame->data[62]);
}



float m10_frame_9f_lat(M10Frame_9f* f) {
	int32_t lat =  f->lat[0] << 24 | f->lat[1] << 16 | f->lat[2] << 8 | f->lat[3];
	return lat * 360.0 / (1UL << 32);
}
float m10_frame_9f_lon(M10Frame_9f* f) {
	int32_t lon = f->lon[0] << 24 | f->lon[1] << 16 | f->lon[2] << 8 | f->lon[3];
	return lon * 360.0 / (1UL << 32);
}
float m10_frame_9f_alt(M10Frame_9f* f) {
	int32_t alt =  f->alt[0] << 24 | f->alt[1] << 16 | f->alt[2] << 8 | f->alt[3];
	return alt / 1e3;
}
float m10_frame_9f_dlat(M10Frame_9f* f) {
	int16_t dlat =  f->dlat[0] << 8 | f->dlat[1];
	return dlat / 200.0;
}
float m10_frame_9f_dlon(M10Frame_9f* f) {
	int16_t dlon =  f->dlon[0] << 8 | f->dlon[1];
	return dlon / 200.0;
}
float m10_frame_9f_dalt(M10Frame_9f* f) {
	int16_t dalt = f->dalt[0] << 8 | f->dalt[1];
	return dalt / 200.0;
}

