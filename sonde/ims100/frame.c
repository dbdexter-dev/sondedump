#include <math.h>
#include <string.h>
#include <time.h>
#include "bitops.h"
#include "frame.h"
#include "physics.h"
#include "utils.h"

static float freq_to_temp(float temp_freq, float ref_freq, const float poly_coeffs[4], const float spline_resists[12], const float spline_temps[12]);
static float freq_to_rh_temp(float temp_rh_freq, float ref_freq, const float poly_coeffs[4], const float r_to_t_coeffs[3]);
static float freq_to_rh(float rh_freq, float ref_freq, float rh_temp, float air_temp, const float poly_coeffs[4]);

/* K0..K6 in the GRUAN docs, found by fitting polynomials to the graphs shown.
 * TODO find the official values somewhere? */
static const float temp_rh_coeffs[] = {5.79231318e-02, -2.64030081e-03, -1.32089353e-05, 7.15251769e-07,
                                       -4.81000481e-04, 1.86628187e+00f, -7.69600770e-01};


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
	uint8_t *const raw_frame = (uint8_t*)frame;
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

			bitcpy(staging, raw_frame, offset, IMS100_MESSAGE_LEN);

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
				bitclear(raw_frame, offset, 2 * IMS100_SUBFRAME_VALUELEN);
			} else if (errdelta) {
				/* Else, copy corrected bits back */
				bitpack(raw_frame, message + start_idx, offset, IMS100_MESSAGE_LEN);
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
	const uint8_t *src = (uint8_t*)ecc_frame;
	uint8_t *dst = (uint8_t*)frame;
	uint32_t validmask = 0;

	/* For each subframe within the frame */
	for (i=0; i < 8 * (int)sizeof(*ecc_frame); i += IMS100_SUBFRAME_LEN) {
		/* For each message in the subframe */
		for (j = 8 * sizeof(ecc_frame->syncword); j < IMS100_SUBFRAME_LEN; j += IMS100_MESSAGE_LEN) {
			offset = i + j;

			/* Copy first message */
			bitcpy(staging, src, offset, IMS100_SUBFRAME_VALUELEN);

			/* Check parity */
			validmask = validmask << 1 | ((count_ones(staging, 2) & 0x1) != staging[2] >> 7 ? 1 : 0);

			/* Copy data bits */
			*dst++ = staging[0];
			*dst++ = staging[1];

			/* Copy second message */
			bitcpy(staging, src, offset + IMS100_SUBFRAME_VALUELEN, IMS100_SUBFRAME_VALUELEN);

			/* Check parity */
			validmask = validmask << 1 | ((count_ones(staging, 2) & 0x1) != staging[2] >> 7 ? 1 : 0);

			/* Copy data bits */
			*dst++ = staging[0];
			*dst++ = staging[1];

		}
	}

	frame->valid = validmask;
}

uint16_t
ims100_frame_seq(const IMS100Frame *frame) {
	return (uint16_t)frame->seq[0] << 8 | frame->seq[1];
}

uint16_t
ims100_frame_subtype(const IMS100Frame *frame)
{
	return (uint16_t)frame->subtype[0] << 8 | frame->subtype[1];
}

float
ims100_frame_temp(const IMS100FrameADC *adc, const IMS100Calibration *calib)
{
	float calib_coeffs[4];
	float calib_temps[12];
	float calib_temp_resists[12];
	int i;

	/* Parse coefficients as BE floats */
	for (i=0; i<4; i++) {
		calib_coeffs[i] = ieee754_be(calib->temp_calib_coeffs[i]);
	}
	for (i=0; i<12; i++) {
		calib_temps[i] = ieee754_be(calib->temps[i]);
		calib_temp_resists[i] = ieee754_be(calib->temp_resists[i]);
	}

	return freq_to_temp(adc->temp, adc->ref, calib_coeffs, calib_temp_resists, calib_temps);
}

float
ims100_frame_rh(const IMS100FrameADC *adc, const IMS100Calibration *calib)
{
	float calib_coeffs[4];
	float rh_temp_calib_coeffs[4];
	float rh_temp_r2t_coeffs[4];
	float air_temp, rh_temp, rh;
	int i;

	/* Parse coefficients as BE floats */
	for (i=0; i<4; i++) {
		calib_coeffs[i] = ieee754_be(calib->rh_calib_coeffs[i]);
		/* TODO technically they should be different for the air temp sensor and
		 * the RH temp sensor, but there are no other coefficients that match in
		 * the config block, so these will have to do for now */
		rh_temp_calib_coeffs[i] = ieee754_be(calib->temp_calib_coeffs[i]);
		rh_temp_r2t_coeffs[i] = ieee754_be(calib->rh_temp_calib_coeffs[i]);
	}

	air_temp = ims100_frame_temp(adc, calib);
	rh_temp = freq_to_rh_temp(adc->rh_temp, adc->ref, rh_temp_calib_coeffs, rh_temp_r2t_coeffs);
	rh = freq_to_rh(adc->rh, adc->ref, rh_temp, air_temp, calib_coeffs);
	return MAX(0, MIN(100, rh));
}

/* Static functions {{{ */
static float
freq_to_temp(float temp_freq, float ref_freq, const float poly_coeffs[4], const float spline_resists[12], const float spline_temps[12])
{
	float f_corrected, r_value, x, t_value;

	f_corrected = 4.0 * temp_freq / ref_freq;
	x = 1.0 / (f_corrected - 1.0);

	r_value = poly_coeffs[0]
	        + poly_coeffs[1] * x
	        + poly_coeffs[2] * x * x
	        + poly_coeffs[3] * x * x * x;

	t_value = cspline(spline_resists, spline_temps, 12, r_value);
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
freq_to_rh(float rh_freq, float ref_freq, float rh_temp, float air_temp, const float poly_coeffs[4])
{
	float f_corrected, x, rh_uncal, temp_correction, rh_cal;

	f_corrected = 4.0 * rh_freq / ref_freq;

	x = f_corrected;
	rh_uncal = poly_coeffs[0]
	         + poly_coeffs[1] * x
	         + poly_coeffs[2] * x * x
	         + poly_coeffs[3] * x * x * x;

	/* Temp correction */
	temp_correction = temp_rh_coeffs[0]
		            + temp_rh_coeffs[1] * rh_temp
		            + temp_rh_coeffs[2] * rh_temp * rh_temp
		            + temp_rh_coeffs[3] * rh_temp * rh_temp * rh_temp;
	temp_correction *= temp_rh_coeffs[4]
		             + temp_rh_coeffs[5] * rh_uncal/100
		             + temp_rh_coeffs[6] * rh_uncal/100 * rh_uncal/100;
	rh_cal = rh_uncal - temp_correction * 100;

	/* If air temp makes sense, use it to correct for different wv sat pressures */
	if (air_temp < 100 && air_temp > -100)  {
		return rh_cal * wv_sat_pressure(rh_temp) / wv_sat_pressure(air_temp);
	}

	/* Otherwise, return as-is */
	return rh_cal;
}
/* }}} */
