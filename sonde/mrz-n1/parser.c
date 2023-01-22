#include <time.h>
#include "bitops.h"
#include "parser.h"
#include "protocol.h"
#include "utils.h"
#include "log/log.h"

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
mrzn1_time(const MRZN1Frame *frame, const MRZN1Calibration *calib)
{
	struct tm datetime;
	time_t time;

	if (calib->date) {
		datetime.tm_hour = frame->hour;
		datetime.tm_min = frame->min;
		datetime.tm_sec = frame->sec;

		datetime.tm_year = 2000 + calib->date % 100 - 1900;
		datetime.tm_mon = (calib->date / 100) % 100 - 1;
		datetime.tm_mday = calib->date / 10000;

		time = my_timegm(&datetime);
	} else {
		time = 3600 * frame->hour + 60 * frame->min + frame->sec;
	}

	return time;
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
	int manuf_year = calib->cal_date % 100;

	sprintf(dst, "MRZ-H1%02d%05d", manuf_year - 10, calib->serial);
}
