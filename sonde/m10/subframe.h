#ifndef m10_subframe_h
#define m10_subframe_h

#include <stdint.h>
#include <time.h>
#include "protocol.h"

/* Subframe field accessors */
time_t m10_frame_9f_time(const M10Frame_9f* f);
void m10_frame_9f_serial(char *dst, const M10Frame_9f *frame);
float m10_frame_9f_lat(const M10Frame_9f* f);
float m10_frame_9f_lon(const M10Frame_9f* f);
float m10_frame_9f_alt(const M10Frame_9f* f);
float m10_frame_9f_dlat(const M10Frame_9f* f);
float m10_frame_9f_dlon(const M10Frame_9f* f);
float m10_frame_9f_dalt(const M10Frame_9f* f);
float m10_frame_9f_temp(const M10Frame_9f* f);
float m10_frame_9f_rh(const M10Frame_9f* f);

time_t m20_frame_20_time(const M20Frame_20* f);
void m20_frame_20_serial(char *dst, const M20Frame_20 *frame);
float m20_frame_20_lat(const M20Frame_20* f);
float m20_frame_20_lon(const M20Frame_20* f);
float m20_frame_20_alt(const M20Frame_20* f);
float m20_frame_20_dlat(const M20Frame_20* f);
float m20_frame_20_dlon(const M20Frame_20* f);
float m20_frame_20_dalt(const M20Frame_20* f);
float m20_frame_20_temp(const M20Frame_20* f);

#endif
