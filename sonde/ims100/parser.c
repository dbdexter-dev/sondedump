#include "parser.h"
#include "physics.h"

static float freq_to_temp(float temp_freq, float ref_freq, const float poly_coeffs[4], const float *spline_resists, const float *spline_temps, size_t spline_len);
static float freq_to_rh_temp(float temp_rh_freq, float ref_freq, const float poly_coeffs[4], const float r_to_t_coeffs[3]);
static float freq_to_rh(float rh_freq, float ref_freq, const float poly_coeffs[4]);


uint16_t
ims100_seq(const IMS100Frame *frame) {
	return (uint16_t)frame->seq[0] << 8 | frame->seq[1];
}



/* IMS100 {{{ */
time_t
ims100_time(const IMS100FrameGPS *frame)
{
	const uint16_t raw_date = (uint16_t)frame->date[0] << 8 | frame->date[1];
	const uint16_t ms = (uint16_t)frame->ms[0] << 8 | frame->ms[1];
	struct tm tm;
	time_t now;
	int year_unit;

	now = time(NULL);
	tm = *gmtime(&now);

	year_unit = tm.tm_year % 10;
	tm.tm_year -= year_unit;
	tm.tm_year += raw_date % 10 - (year_unit < raw_date % 10 ? 10 : 0);

	tm.tm_mon = (raw_date / 10) % 100 - 1;
	tm.tm_mday = raw_date / 1000;
	tm.tm_hour = frame->hour;
	tm.tm_min = frame->min;
	tm.tm_sec = ms / 1000;

	return my_timegm(&tm);
}

float
ims100_lat(const IMS100FrameGPS *frame)
{
	const int32_t raw_lat = ((int32_t)frame->lat[0] << 24
	                      | (int32_t)frame->lat[1] << 16
	                      | (int32_t)frame->lat[2] << 8
	                      | (int32_t)frame->lat[3]);
	/* Convert NMEA to decimal degrees */
	float lat = (int)(raw_lat / 1e6) + (raw_lat % 1000000 / 60.0 * 100.0) / 1e6;

	return lat;

}

float
ims100_lon(const IMS100FrameGPS *frame)
{
	const int32_t raw_lon = ((int32_t)frame->lon[0] << 24
	                      | (int32_t)frame->lon[1] << 16
	                      | (int32_t)frame->lon[2] << 8
	                      | (int32_t)frame->lon[3]);
	/* Convert NMEA to decimal degrees */
	float lon = (int)(raw_lon / 1e6) + (raw_lon % 1000000 / 60.0 * 100.0) / 1e6;

	return lon;
}

float
ims100_alt(const IMS100FrameGPS *frame)
{
	const int32_t raw_alt = (int32_t)frame->alt[0] << 24
	                      | (int32_t)frame->alt[1] << 16
	                      | (int32_t)frame->alt[2] << 8;

	return (raw_alt >> 8) / 1e2;
}

float
ims100_speed(const IMS100FrameGPS *frame)
{
	const uint16_t raw_speed = (uint16_t)frame->speed[0] << 8 | (uint16_t)frame->speed[1];

	return raw_speed / 1.943844e2;  // knots*100 -> m/s
}

float
ims100_heading(const IMS100FrameGPS *frame)
{
	const int16_t raw_heading = (int16_t)frame->heading[0] << 8 | (int16_t)frame->heading[1];

	return abs(raw_heading) / 1e2;
}

float
ims100_temp(const IMS100FrameADC *adc, const IMS100Calibration *calib)
{
	float temp_poly[4];

	/* FIXME this does not make sense, but for some reason it works? */
	temp_poly[0] = calib->temp_poly[0] - calib->temp_poly[3];
	temp_poly[1] = calib->temp_poly[1];
	temp_poly[2] = calib->temp_poly[2];
	temp_poly[3] = 0;

	return 1 + freq_to_temp(adc->temp, adc->ref,
	                        temp_poly, calib->temp_resists, calib->temps, LEN(calib->temps));
}

float
ims100_rh(const IMS100FrameADC *adc, const IMS100Calibration *calib)
{
	float air_temp, rh_temp, rh;
	float temp_poly[4];

	/* FIXME this does not make sense, but for some reason it works? */
	temp_poly[0] = calib->temp_poly[0] - calib->temp_poly[3];
	temp_poly[1] = calib->temp_poly[1];
	temp_poly[2] = calib->temp_poly[2];
	temp_poly[3] = 0;

	air_temp = ims100_temp(adc, calib);
	rh_temp = 1 + freq_to_rh_temp(adc->rh_temp, adc->ref, temp_poly, calib->rh_temp_poly);
	rh = freq_to_rh(adc->rh, adc->ref, calib->rh_poly);

	/* If air temp makes sense, use it to correct for different wv sat pressures */
	if (air_temp < 100 && air_temp > -100)  {
		rh *= wv_sat_pressure(rh_temp) / wv_sat_pressure(air_temp);
	}

	return MAX(0, MIN(100, rh));
}
/* }}} */

/* RS-11G {{{ */
time_t
rs11g_date(const RS11GFrameGPS *frame)
{
	struct tm tm;

	tm.tm_year = frame->year + 0x700 - 1900;
	tm.tm_mon = frame->month - 1;
	tm.tm_mday = frame->day;

	tm.tm_hour = 0;
	tm.tm_min = 0;
	tm.tm_sec = 0;

	return my_timegm(&tm);
}


float
rs11g_lat(const RS11GFrameGPS *frame)
{
	const int32_t raw_lat = ((int32_t)frame->lat[0] << 24
	                      | (int32_t)frame->lat[1] << 16
	                      | (int32_t)frame->lat[2] << 8
	                      | (int32_t)frame->lat[3]);
	/* Convert NMEA to decimal degrees */
	float lat = raw_lat / 1e7;

	return lat;

}

float
rs11g_lon(const RS11GFrameGPS *frame)
{
	const int32_t raw_lon = ((int32_t)frame->lon[0] << 24
	                      | (int32_t)frame->lon[1] << 16
	                      | (int32_t)frame->lon[2] << 8
	                      | (int32_t)frame->lon[3]);
	/* Convert NMEA to decimal degrees */
	float lon = raw_lon / 1e7;

	return lon;
}

float
rs11g_alt(const RS11GFrameGPS *frame)
{
	const int32_t raw_alt = (int32_t)frame->alt[0] << 24
	                      | (int32_t)frame->alt[1] << 16
	                      | (int32_t)frame->alt[2] << 8
	                      | (int32_t)frame->alt[3];

	return raw_alt / 1e2;
}

float
rs11g_speed(const RS11GFrameGPS *frame)
{
	const uint16_t raw_speed = (uint16_t)frame->speed[0] << 8 | (uint16_t)frame->speed[1];

	return raw_speed / 1e2;
}

float
rs11g_heading(const RS11GFrameGPS *frame)
{
	const int16_t raw_heading = (int16_t)frame->heading[0] << 8 | (int16_t)frame->heading[1];

	return abs(raw_heading) / 1e2;
}

float
rs11g_climb(const RS11GFrameGPS *frame)
{
	const int16_t raw_climb = (int16_t)frame->climb[0] << 8 | (int16_t)frame->climb[1];

	return abs(raw_climb) / 1e2;
}

time_t
rs11g_time(const RS11GFrameGPSRaw *frame)
{
	const uint16_t ms = (uint16_t)frame->ms[0] | frame->ms[1] << 8;
	struct tm tm;

	tm.tm_hour = frame->hour;
	tm.tm_min = frame->min;
	tm.tm_sec = ms / 1000;

	return tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;
}

float
rs11g_temp(const IMS100FrameADC *adc, const RS11GCalibration *calib)
{
	return freq_to_temp(adc->temp, adc->ref,
	                    calib->temp_poly, calib->temp_resists, calib->temps, LEN(calib->temps));
}

float
rs11g_rh(const IMS100FrameADC *adc, const RS11GCalibration *calib)
{
	/* K0..K6 in the GRUAN docs, found by fitting polynomials to the graphs shown.*/
	const float temp_rh_coeffs[] = {5.79231318e-02, -2.64030081e-03, -1.32089353e-05, 7.15251769e-07,
	                                -4.81000481e-04, 1.86628187e+00, -7.69600770e-01};

	float air_temp, rh, rh_cal, temp_correction;

	air_temp = rs11g_temp(adc, calib);
	rh = freq_to_rh(adc->rh, adc->ref, calib->rh_poly);

	/* RS-11G specific temp correction, assuming that air and sensor temp are
	 * the same. Taken from GRUAN TD-5 by curve fitting the graphs shown */
	temp_correction = temp_rh_coeffs[0]
		            + temp_rh_coeffs[1] * air_temp
		            + temp_rh_coeffs[2] * air_temp * air_temp
		            + temp_rh_coeffs[3] * air_temp * air_temp * air_temp;
	temp_correction *= temp_rh_coeffs[4]
		             + temp_rh_coeffs[5] * rh/100
		             + temp_rh_coeffs[6] * rh/100 * rh/100;
	rh_cal = rh + temp_correction * 100;

	return MAX(0, MIN(100, rh_cal));
}
/* }}} */

/* Static functions {{{ */
static float
freq_to_temp(float temp_freq, float ref_freq, const float poly_coeffs[4], const float *spline_resists, const float *spline_temps, size_t spline_len)
{
	float f_corrected, r_value, x, t_value;
	float log_resists[12];
	size_t i;

	f_corrected = 4.0 * temp_freq / ref_freq;
	x = 1.0 / (f_corrected - 1.0);

	r_value = poly_coeffs[0]
	        + poly_coeffs[1] * x
	        + poly_coeffs[2] * x * x
	        + poly_coeffs[3] * x * x * x;

	/* Convert resistance values to their natural log, then interpolate the temp */
	for (i=0; i<spline_len; i++) {
		log_resists[i] = logf(spline_resists[i]);
	}

	t_value = cspline(log_resists, spline_temps, spline_len, logf(r_value));
	return MAX(-100, MIN(100, t_value));
}

static float
freq_to_rh_temp(float temp_rh_freq, float ref_freq, const float poly_coeffs[4], const float r_to_t_coeffs[3])
{
	float f_corrected, r_value, x, t_rh_value;

	f_corrected = 4.0 * temp_rh_freq / ref_freq;
	x = 1.0 / (f_corrected - 1.0);

	r_value = poly_coeffs[0]
	        + poly_coeffs[1] * x
	        + poly_coeffs[2] * x * x
	        + poly_coeffs[3] * x * x * x;

	x = logf(r_value);
	t_rh_value = 1.0 / (r_to_t_coeffs[0] * x * x * x
	                  + r_to_t_coeffs[1] * x
	                  + r_to_t_coeffs[2])
	           - 273.15;

	return t_rh_value;
}

static float
freq_to_rh(float rh_freq, float ref_freq, const float poly_coeffs[4])
{
	float freq, rh;

	freq = 4.0 * rh_freq / ref_freq;

	rh = poly_coeffs[0]
	   + poly_coeffs[1] * freq
	   + poly_coeffs[2] * freq * freq
	   + poly_coeffs[3] * freq * freq * freq;

	return rh;
}
/* }}} */
