#ifndef m10_subframe_h
#define m10_subframe_h

#include <stdint.h>
#include <time.h>
#include "protocol.h"

/* Subframe field accessors */
time_t m10_frame_9f_time(M10Frame_9f* f);
void m10_frame_9f_serial(char *dst, M10Frame_9f *frame);
float m10_frame_9f_lat(M10Frame_9f* f);
float m10_frame_9f_lon(M10Frame_9f* f);
float m10_frame_9f_alt(M10Frame_9f* f);
float m10_frame_9f_dlat(M10Frame_9f* f);
float m10_frame_9f_dlon(M10Frame_9f* f);
float m10_frame_9f_dalt(M10Frame_9f* f);

void m20_frame_20_serial(char *dst, M20Frame_20 *frame);

float m20_frame_20_lat(M20Frame_20* f);
float m20_frame_20_lon(M20Frame_20* f);
float m20_frame_20_alt(M20Frame_20* f);
float m20_frame_20_dlat(M20Frame_20* f);
float m20_frame_20_dlon(M20Frame_20* f);
float m20_frame_20_dalt(M20Frame_20* f);

#endif
