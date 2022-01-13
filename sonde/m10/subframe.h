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


#endif
