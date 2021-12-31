#include "protocol.h"
#include "gps/time.h"

time_t m10_frame_9f_time(M10Frame_9f* f) {
	const uint32_t ms = f->time[0] << 24 | f->time[1] << 16 | f->time[2] << 8 | f->time[3];
	const uint16_t week = f->week[0] << 8 | f->week[1];

	return gps_time_to_utc(week, ms);
}
extern inline int32_t m10_frame_9f_lat(M10Frame_9f* f);
extern inline int32_t m10_frame_9f_lon(M10Frame_9f* f);
extern inline int32_t m10_frame_9f_alt(M10Frame_9f* f);

extern inline int16_t m10_frame_9f_dlat(M10Frame_9f* f);
extern inline int16_t m10_frame_9f_dlon(M10Frame_9f* f);
extern inline int16_t m10_frame_9f_dalt(M10Frame_9f* f);
