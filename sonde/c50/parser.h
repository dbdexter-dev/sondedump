#ifndef c50_parser_h
#define c50_parser_h

#include <stdint.h>
#include <time.h>
#include "protocol.h"

float c50_temp(const C50Frame *frame);

float c50_lat(const C50Frame *frame);
float c50_lon(const C50Frame *frame);
float c50_alt(const C50Frame *frame);

void c50_date(struct tm *date, const C50Frame *frame);
void c50_time(struct tm *time, const C50Frame *frame);
void c50_serial(char *dst, const C50Frame *frame);

#endif
