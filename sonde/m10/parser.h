#ifndef m10_parser_h
#define m10_parser_h

#include <stdint.h>
#include <time.h>
#include "protocol.h"

/* Subframe field accessors */
time_t m10_9f_time(const M10Frame_9f* f);
void m10_9f_serial(char *dst, const M10Frame_9f *frame);
float m10_9f_lat(const M10Frame_9f* f);
float m10_9f_lon(const M10Frame_9f* f);
float m10_9f_alt(const M10Frame_9f* f);
float m10_9f_dlat(const M10Frame_9f* f);
float m10_9f_dlon(const M10Frame_9f* f);
float m10_9f_dalt(const M10Frame_9f* f);
float m10_9f_temp(const M10Frame_9f* f);
float m10_9f_rh(const M10Frame_9f* f);

time_t m20_20_time(const M20Frame_20* f);
void m20_20_serial(char *dst, const M20Frame_20 *frame);
float m20_20_lat(const M20Frame_20* f);
float m20_20_lon(const M20Frame_20* f);
float m20_20_alt(const M20Frame_20* f);
float m20_20_dlat(const M20Frame_20* f);
float m20_20_dlon(const M20Frame_20* f);
float m20_20_dalt(const M20Frame_20* f);
float m20_20_temp(const M20Frame_20* f);

#endif
