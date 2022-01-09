#include <math.h>
#include <string.h>
#include <time.h>
#include "frame.h"

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
	float rt_freq, rt_resist, rt_temp, x, linear_percent;
	int i;

	/* Parse coefficients as BE floats */
	for (i=0; i<4; i++) {
		calib_coeffs[i] = ieee754_be(calib->calib_coeffs[i][1]);
	}
	for (i=0; i<12; i++) {
		calib_temps[i] = ieee754_be(calib->temps[i]);
		calib_temp_resists[i] = ieee754_be(calib->temp_resists[i]);
	}

	/* Convert ADC values to resistance frequency */
	rt_freq = 4.0 * adc_val / adc_ref;
	x = 1.0 / (rt_freq - 1.0);

	/* Convert resistance frequency to resistance value */
	rt_resist = calib_coeffs[3]
	          + calib_coeffs[2] * x
	          + calib_coeffs[1] * x * x
	          + calib_coeffs[0] * x * x * x;
	//rt_resist = logf(rt_resist);


	/* Spline our way from resistance to temperature. TODO use an actual spline */
	rt_temp = NAN;
	for (i=0; i<12 - 1; i++) {
		if (rt_resist < calib_temp_resists[i+1]) {
			linear_percent = (rt_resist - calib_temp_resists[i]) / (calib_temp_resists[i+1] - calib_temp_resists[i]);
			rt_temp = calib_temps[i] + (calib_temps[i+1] - calib_temps[i]) * linear_percent;
			rt_temp = MAX(-100, MIN(100, rt_temp));
			break;
		}
	}

	return rt_temp;
}

float
IMS100Frame_rh(const IMS100Frame *frame, const IMS100Calibration *calib)
{
#if 0
	const float adc_val = (uint16_t)frame->adc_rh[0] << 8 | frame->adc_rh[1];
	const float adc_ref = (uint16_t)frame->adc_ref[0] << 8 | frame->adc_ref[1];
	float calib_coeffs[4];
	float rh_freq, rh_humidity;
	int i;

	if (!IMS100_DATA_VALID(frame->valid, IMS100_EVEN_MASK_PTU)) return NAN;

	for (i=0; i<4; i++) {
		calib_coeffs[i] = ieee754_be(calib->coeffs[i]);
	}

	rh_freq = 4.0 * adc_val / adc_ref;

	rh_humidity = calib_coeffs[3]
	            + calib_coeffs[2] * rh_freq
	            + calib_coeffs[1] * rh_freq * rh_freq
	            + calib_coeffs[0] * rh_freq * rh_freq * rh_freq;


	return MAX(0, MIN(100, rh_humidity * 100.0));
#else
	return 0;
#endif
}

