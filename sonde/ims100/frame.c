#include <string.h>
#include <time.h>
#include "frame.h"

static uint32_t ims100_unpack_internal(uint8_t *dst, const IMS100Frame *src);

void
ims100_frame_descramble(IMS100Frame *frame)
{
	int i;
	uint8_t *raw_frame = (uint8_t*)frame;

	for (i=0; i<(int)sizeof(*frame); i++) {
		raw_frame[i] ^= raw_frame[i] << 1 | raw_frame[i+1] >> 7;
	}
}

int
ims100_frame_error_correct(IMS100Frame *frame, RSDecoder *rs)
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
ims100_frame_unpack_even(IMS100FrameEven *dst, IMS100Frame *src)
{
	dst->valid = ims100_unpack_internal((uint8_t*)dst, src);
}

void
ims100_frame_unpack_odd(IMS100FrameOdd *dst, IMS100Frame *src)
{
	dst->valid = ims100_unpack_internal((uint8_t*)dst->seq, src);
}

static uint32_t
ims100_unpack_internal(uint8_t *dst, const IMS100Frame *frame)
{
	int i, j, offset;
	uint8_t staging[3];
	const uint8_t *src = (uint8_t*)frame;
	uint32_t validmask = 0;

	/* For each subframe within the frame */
	for (i=0; i < 8 * (int)sizeof(*frame); i += IMS100_SUBFRAME_LEN) {
		/* For each message in the subframe */
		for (j = 8 * sizeof(frame->syncword); j < IMS100_SUBFRAME_LEN; j += IMS100_MESSAGE_LEN) {
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

	return validmask;
}

uint16_t IMS100Frame_seq(const IMS100Frame *frame) {
	return (uint16_t)frame->seq[0] << 8 | frame->seq[1];
}

uint16_t IMS100FrameEven_seq(const IMS100FrameEven *frame) {
	if (!IMS100_DATA_VALID(frame->valid, IMS100_EVEN_MASK_SEQ)) return 0;

	return (uint16_t)frame->seq[0] << 8 | frame->seq[1];
}

time_t IMS100FrameEven_time(const IMS100FrameEven *frame) {
	if (!IMS100_DATA_VALID(frame->valid, IMS100_EVEN_MASK_TIME | IMS100_EVEN_MASK_DATE)) return 0;

	const uint16_t raw_date = (uint16_t)frame->date[0] << 8 | frame->date[1];
	const uint16_t ms = (uint16_t)frame->ms[0] << 8 | frame->ms[1];
	struct tm tm;
	time_t now;
	int year_unit;

	now = time(NULL);
	gmtime_r(&now, &tm);

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

float IMS100FrameEven_lat(const IMS100FrameEven *frame) {
	if (!IMS100_DATA_VALID(frame->valid, IMS100_EVEN_MASK_LAT)) return NAN;

	int32_t raw_lat = ((int32_t)frame->lat[0] << 24
	                | (int32_t)frame->lat[1] << 16
	                | (int32_t)frame->lat[2] << 8
	                | (int32_t)frame->lat[3]);

	return raw_lat / 1e6;

}

float IMS100FrameEven_lon(const IMS100FrameEven *frame) {
	if (!IMS100_DATA_VALID(frame->valid, IMS100_EVEN_MASK_LON)) return NAN;

	int32_t raw_lon = ((int32_t)frame->lon[0] << 24
	                | (int32_t)frame->lon[1] << 16
	                | (int32_t)frame->lon[2] << 8
	                | (int32_t)frame->lon[3]);

	return raw_lon / 1e6;
}

float IMS100FrameEven_alt(const IMS100FrameEven *frame) {
	if (!IMS100_DATA_VALID(frame->valid, IMS100_EVEN_MASK_ALT)) return NAN;

	int32_t raw_alt = (int32_t)frame->alt[0] << 24
	                | (int32_t)frame->alt[1] << 16
	                | (int32_t)frame->alt[2] << 8;

	return (raw_alt >> 8) / 1e2;
}

float IMS100FrameEven_speed(const IMS100FrameEven *frame) {
	if (!IMS100_DATA_VALID(frame->valid, IMS100_EVEN_MASK_SPEED)) return NAN;

	uint16_t raw_speed = (uint16_t)frame->speed[0] << 8 | (uint16_t)frame->speed[1];

	return raw_speed / 3.280840e2;  // Feet per second what the...? But empirically plausible
}

float IMS100FrameEven_heading(const IMS100FrameEven *frame) {
	if (!IMS100_DATA_VALID(frame->valid, IMS100_EVEN_MASK_HEADING)) return NAN;

	int16_t raw_heading = (int16_t)frame->heading[0] << 8 | (int16_t)frame->heading[1];

	return raw_heading / 1e2;
}


uint16_t IMS100FrameOdd_seq(const IMS100FrameOdd *frame) {
	if (!IMS100_DATA_VALID(frame->valid, IMS100_EVEN_MASK_SEQ)) return 0;

	return (uint16_t)frame->seq[0] << 8 | frame->seq[1];
}
