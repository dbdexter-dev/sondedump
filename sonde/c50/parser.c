#include <bitops.h>
#include "parser.h"

float
c50_temp(const C50Frame *frame)
{
	return ieee754_be(frame->data);
}

float
c50_lat(const C50Frame *frame)
{
	uint32_t raw = frame->data[0] << 24 | frame->data[1] << 16 | frame->data[2] << 8 | frame->data[3];
	return (int)((int32_t)raw / 1e7) + ((int32_t)raw % 10000000 / 60.0 * 100.0) / 1e7;
}

float
c50_lon(const C50Frame *frame)
{
	uint32_t raw = frame->data[0] << 24 | frame->data[1] << 16 | frame->data[2] << 8 | frame->data[3];
	return (int)((int32_t)raw / 1e7) + ((int32_t)raw % 10000000 / 60.0 * 100.0) / 1e7;
}

float
c50_alt(const C50Frame *frame)
{
	uint32_t raw = frame->data[0] << 24 | frame->data[1] << 16 | frame->data[2] << 8 | frame->data[3];
	return (int32_t)raw / 10.0f;
}

void
c50_date(struct tm *date, const C50Frame *frame)
{
	uint32_t raw = frame->data[0] << 24 | frame->data[1] << 16 | frame->data[2] << 8 | frame->data[3];
	date->tm_mday = raw / 10000 % 100;
	date->tm_mon = (raw / 100) % 100 - 1;
	date->tm_year = 2000 + raw % 100 - 1900;
}

void
c50_time(struct tm *time, const C50Frame *frame)
{
	uint32_t raw = frame->data[0] << 24 | frame->data[1] << 16 | frame->data[2] << 8 | frame->data[3];

	time->tm_hour = raw / 10000;
	time->tm_min = (raw / 100) % 100;
	time->tm_sec = raw % 100;
}

void
c50_serial(char *dst, const C50Frame *frame)
{
	uint32_t raw = frame->data[0] << 24 | frame->data[1] << 16 | frame->data[2] << 8 | frame->data[3];

	sprintf(dst, "C50-%d", raw);
}
