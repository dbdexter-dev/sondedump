#ifndef ims100_parser_h
#define ims100_parser_h
#include "protocol.h"

typedef struct {
	uint16_t ref, temp, rh, rh_temp;
} IMS100FrameADC;

/* Common between RS-11G and iMS-100 */
uint16_t ims100_seq(const IMS100Frame *frame);

/* Accessors for GPS subframe fields */
time_t ims100_time(const IMS100FrameGPS *frame);
float ims100_lat(const IMS100FrameGPS *frame);
float ims100_lon(const IMS100FrameGPS *frame);
float ims100_alt(const IMS100FrameGPS *frame);
float ims100_speed(const IMS100FrameGPS *frame);
float ims100_heading(const IMS100FrameGPS *frame);

float ims100_temp(const IMS100FrameADC *adc, const IMS100Calibration *calib);
float ims100_rh(const IMS100FrameADC *adc, const IMS100Calibration *calib);

time_t rs11g_date(const RS11GFrameGPS *frame);
float rs11g_lat(const RS11GFrameGPS *frame);
float rs11g_lon(const RS11GFrameGPS *frame);
float rs11g_alt(const RS11GFrameGPS *frame);
float rs11g_speed(const RS11GFrameGPS *frame);
float rs11g_heading(const RS11GFrameGPS *frame);
float rs11g_climb(const RS11GFrameGPS *frame);

time_t rs11g_time(const RS11GFrameGPSRaw *frame);

float rs11g_temp(const IMS100FrameADC *adc, const RS11GCalibration *calib);
float rs11g_rh(const IMS100FrameADC *adc, const RS11GCalibration *calib);


#endif
