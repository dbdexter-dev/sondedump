#include <math.h>
#include <string.h>
#include <time.h>
#include "bitops.h"
#include "frame.h"
#include "physics.h"
#include "utils.h"
#include "log/log.h"

static float freq_to_temp(float temp_freq, float ref_freq, const float poly_coeffs[4], const float *spline_resists, const float *spline_temps, size_t spline_len);
static float freq_to_rh_temp(float temp_rh_freq, float ref_freq, const float poly_coeffs[4], const float r_to_t_coeffs[3]);
static float freq_to_rh(float rh_freq, float ref_freq, const float poly_coeffs[4]);

void
ims100_frame_descramble(IMS100ECCFrame *frame)
{
	int i;
	uint8_t *raw_frame = (uint8_t*)frame;

	for (i=0; i<(int)sizeof(*frame); i++) {
		raw_frame[i] ^= raw_frame[i] << 1 | raw_frame[i+1] >> 7;
	}
}

int
ims100_frame_error_correct(IMS100ECCFrame *frame, const RSDecoder *rs)
{
	const int start_idx = IMS100_REEDSOLOMON_N - IMS100_MESSAGE_LEN;
	int i, j, k;
	int offset;
	int errcount, errdelta;
	uint8_t staging[IMS100_MESSAGE_LEN/8+1];
	/* FIXME this should not need the +1, but Windows compiled versions get
	 * angry because of some buffer overflow. Check BCH code, one of the roots
	 * can be 63 which is past the end of buffer */
	uint8_t message[IMS100_REEDSOLOMON_N + 1];

	errcount = 0;
	memset(message, 0, sizeof(message));

	/* For each subframe within the frame */
	for (i=0; i < 8 * (int)sizeof(*frame); i += IMS100_SUBFRAME_LEN) {
		/* For each message in the subframe */
		for (j=8*sizeof(frame->syncword); j < IMS100_SUBFRAME_LEN; j += IMS100_MESSAGE_LEN) {
			offset = i + j;

			bitcpy(staging, frame, offset, IMS100_MESSAGE_LEN);

			/* Expand bits to bytes, zero-padding to a (64,51) code */
			for (k=0; k<IMS100_MESSAGE_LEN; k++) {
				message[start_idx + k] = (staging[k/8] >> (7 - k%8)) & 0x1;
			}

			/* Error correct */
			errdelta = rs_fix_block(rs, message);
			if (errdelta < 0 || errcount < 0) {
				errcount = -1;
			} else {
				errcount += errdelta;
			}

			if (errdelta < 0) {
				/* If ECC fails, clear the message */
				bitclear(frame, offset, 2 * IMS100_SUBFRAME_VALUELEN);
			} else if (errdelta) {
				/* Else, copy corrected bits back */
				bitpack(frame, message + start_idx, offset, IMS100_MESSAGE_LEN);
			}
		}
	}

	return errcount;
}

void
ims100_frame_unpack(IMS100Frame *frame, const IMS100ECCFrame *ecc_frame)
{
	int i, j, offset;
	uint8_t staging[3];
	uint8_t *dst = (uint8_t*)frame;
	uint32_t validmask = 0;

	/* For each subframe within the frame */
	for (i=0; i < 8 * (int)sizeof(*ecc_frame); i += IMS100_SUBFRAME_LEN) {
		/* For each message in the subframe */
		for (j = 8 * sizeof(ecc_frame->syncword); j < IMS100_SUBFRAME_LEN; j += IMS100_MESSAGE_LEN) {
			offset = i + j;

			/* Copy first message */
			bitcpy(staging, ecc_frame, offset, IMS100_SUBFRAME_VALUELEN);

			/* Check parity */
			validmask = validmask << 1 | ((count_ones(staging, 2) & 0x1) != staging[2] >> 7 ? 1 : 0);

			/* Copy data bits */
			*dst++ = staging[0];
			*dst++ = staging[1];

			/* Copy second message */
			bitcpy(staging, ecc_frame, offset + IMS100_SUBFRAME_VALUELEN, IMS100_SUBFRAME_VALUELEN);

			/* Check parity */
			validmask = validmask << 1 | ((count_ones(staging, 2) & 0x1) != staging[2] >> 7 ? 1 : 0);

			/* Copy data bits */
			*dst++ = staging[0];
			*dst++ = staging[1];

		}
	}

	frame->valid = validmask;
	if (validmask != 0xFFFFFF) {
		log_debug("Validity mask: %06x", validmask);
		log_debug_hexdump(frame, sizeof(*frame));
	}
}

uint16_t
ims100_frame_seq(const IMS100Frame *frame) {
	return (uint16_t)frame->seq[0] << 8 | frame->seq[1];
}

float
ims100_frame_temp(const IMS100FrameADC *adc, const IMS100Calibration *calib)
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
ims100_frame_rh(const IMS100FrameADC *adc, const IMS100Calibration *calib)
{
	float air_temp, rh_temp, rh;
	float temp_poly[4];

	/* FIXME this does not make sense, but for some reason it works? */
	temp_poly[0] = calib->temp_poly[0] - calib->temp_poly[3];
	temp_poly[1] = calib->temp_poly[1];
	temp_poly[2] = calib->temp_poly[2];
	temp_poly[3] = 0;

	air_temp = ims100_frame_temp(adc, calib);
	rh_temp = 1 + freq_to_rh_temp(adc->rh_temp, adc->ref, temp_poly, calib->rh_temp_poly);
	rh = freq_to_rh(adc->rh, adc->ref, calib->rh_poly);

	/* If air temp makes sense, use it to correct for different wv sat pressures */
	if (air_temp < 100 && air_temp > -100)  {
		rh *= wv_sat_pressure(rh_temp) / wv_sat_pressure(air_temp);
	}

	return MAX(0, MIN(100, rh));
}

float
rs11g_frame_temp(const IMS100FrameADC *adc, const RS11GCalibration *calib)
{
	return freq_to_temp(adc->temp, adc->ref,
	                    calib->temp_poly, calib->temp_resists, calib->temps, LEN(calib->temps));
}

float
rs11g_frame_rh(const IMS100FrameADC *adc, const RS11GCalibration *calib)
{
	/* K0..K6 in the GRUAN docs, found by fitting polynomials to the graphs shown.*/
	const float temp_rh_coeffs[] = {5.79231318e-02, -2.64030081e-03, -1.32089353e-05, 7.15251769e-07,
	                                -4.81000481e-04, 1.86628187e+00, -7.69600770e-01};

	float air_temp, rh, rh_cal, temp_correction;

	air_temp = rs11g_frame_temp(adc, calib);
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
