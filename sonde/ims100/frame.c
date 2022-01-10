#include <math.h>
#include <string.h>
#include <time.h>
#include "frame.h"
#include "utils.h"

static float freq_to_temp(float temp_freq, float ref_freq, const float poly_coeffs[4], const float spline_resists[12], const float spline_temps[12]);
static float freq_to_temp_rh(float temp_rh_freq, float ref_freq, const float poly_coeffs[4], const float r_to_t_coeffs[3]);
static float freq_to_rh(float rh_freq, float ref_freq, float rh_temp, float air_temp, const float poly_coeffs[4]);


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
ims100_frame_error_correct(IMS100ECCFrame *frame, RSDecoder *rs)
{
	const int start_idx = IMS100_REEDSOLOMON_N - IMS100_MESSAGE_LEN;
	int i, j, k;
	int offset;
	int errcount, errdelta;
	uint8_t *const raw_frame = (uint8_t*)frame;
	uint8_t staging[IMS100_MESSAGE_LEN/8+1];
	uint8_t message[IMS100_REEDSOLOMON_N];

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
			} else if (errdelta) {
				/* If an error was "corrected" in the padding, count it as a
				 * failure to correct */
				for (k=0; k<start_idx; k++) {
					if (message[i] != 0x00) {
						errcount = -1;
						errdelta = -1;
						break;
					}
				}

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
IMS100Frame_seq(const IMS100Frame *frame) {
	if (!IMS100_DATA_VALID(frame->valid, IMS100_MASK_SEQ)) return -1;

	return (uint16_t)frame->seq[0] << 8 | frame->seq[1];
}

uint16_t
IMS100Frame_subtype(const IMS100Frame *frame)
{
	if (!IMS100_DATA_VALID(frame->valid, IMS100_MASK_SUBTYPE)) return -1;

	return (uint16_t)frame->subtype[0] << 8 | frame->subtype[1];
}

float
IMS100Frame_temp(const IMS100Frame *frame, const IMS100Calibration *calib)
{
	const float adc_val = (uint16_t)frame->adc_temp[0] << 8 | frame->adc_temp[1];
	const float adc_ref = (uint16_t)frame->adc_ref[0] << 8 | frame->adc_ref[1];
	float calib_coeffs[4];
	float calib_temps[12];
	float calib_temp_resists[12];
	int i;

	/* Parse coefficients as BE floats */
	for (i=0; i<4; i++) {
		calib_coeffs[i] = ieee754_be(calib->calib_coeffs[4 + i]);
	}
	for (i=0; i<12; i++) {
		calib_temps[i] = ieee754_be(calib->temps[i]);
		calib_temp_resists[i] = ieee754_be(calib->temp_resists[i]);
	}

	return freq_to_temp(adc_val, adc_ref, calib_coeffs, calib_temp_resists, calib_temps);
}

float
IMS100Frame_rh(const IMS100Frame *frame, const IMS100Calibration *calib)
{
	const float adc_val = (uint16_t)frame->adc_rh[0] << 8 | frame->adc_rh[1];
	const float adc_ref = (uint16_t)frame->adc_ref[0] << 8 | frame->adc_ref[1];
	const float air_temp = IMS100Frame_temp(frame, calib);
	float calib_coeffs[4];
	int i;

	/* Parse coefficients as BE floats */
	for (i=0; i<4; i++) {
		calib_coeffs[i] = ieee754_be(calib->calib_coeffs[i]);
	}

	return freq_to_rh(adc_val, adc_ref, 0, air_temp, calib_coeffs);
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
freq_to_temp_rh(float temp_rh_freq, float ref_freq, const float poly_coeffs[4], const float r_to_t_coeffs[3])
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
	float f_corrected, x, rh_uncal, rh_cal;

	f_corrected = 4.0 * rh_freq / ref_freq;

	x = f_corrected;
	rh_uncal = poly_coeffs[0]
	         + poly_coeffs[1] * x
	         + poly_coeffs[2] * x * x
	         + poly_coeffs[3] * x * x * x;

	/* TODO temp correction, where the heck are the coefficients? */
	return rh_uncal;
	return rh_cal * wv_sat_pressure(rh_temp) / wv_sat_pressure(air_temp);
}
/* }}} */
