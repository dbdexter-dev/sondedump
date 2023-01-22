#include <time.h>
#include "bitops.h"
#include "parser.h"
#include "protocol.h"
#include "utils.h"

int
mrzn1_seq(const MRZN1Frame *frame)
{
	return frame->seq & 0x0F;
}

int
mrzn1_calib_seq(const MRZN1Frame *frame)
{
	return frame->calib_frag_seq - 1;
}

time_t
mrzn1_time(const MRZN1Frame *frame)
{
	struct tm datetime;
	time_t now;

	now = time(NULL);
	datetime = *gmtime(&now);

	if (abs(frame->hour - datetime.tm_hour) >= 12) {
		now += (frame->hour < datetime.tm_hour) ? 86400 : -86400;
		datetime = *gmtime(&now);
	}

	datetime.tm_hour = frame->hour;
	datetime.tm_min = frame->min;
	datetime.tm_sec = frame->sec;

	return my_timegm(&datetime);
}

float
mrzn1_x(const MRZN1Frame *frame)
{
	return frame->x / 100.0f;
}
float
mrzn1_y(const MRZN1Frame *frame)
{
	return frame->y / 100.0f;
}
float
mrzn1_z(const MRZN1Frame *frame)
{
	return frame->z / 100.0f;
}
float
mrzn1_dx(const MRZN1Frame *frame)
{
	return frame->dx / 100.0f;
}
float
mrzn1_dy(const MRZN1Frame *frame)
{
	return frame->dy / 100.0f;
}
float
mrzn1_dz(const MRZN1Frame *frame)
{
	return frame->dz / 100.0f;
}

float
mrzn1_temp(const MRZN1Frame *frame)
{
	return frame->temp / 100.0f;
}
float
mrzn1_rh(const MRZN1Frame *frame)
{
	return frame->rh / 100.0f;
}

void
mrzn1_serial(char *dst, const MRZN1Calibration *calib)
{
	/* TODO */
	int manuf_year = 0;
	int serial = 0;

	sprintf(dst, "3МК%01d%05dK", manuf_year % 10, serial);
}
