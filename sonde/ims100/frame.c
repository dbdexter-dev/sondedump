#include <string.h>
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
			if (errdelta < 0 || errcount < 0) errcount = -1;
			else errcount += errdelta;

			/* Move back corrected bits */
			bitpack(raw_frame, message + start_idx, offset, IMS100_MESSAGE_LEN);
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
ims100_frame_unpack_odd(IMS100FrameEven *dst, IMS100Frame *src)
{
	dst->valid = ims100_unpack_internal((uint8_t*)dst, src);
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
		for (j= 8 * sizeof(frame->syncword); j < IMS100_SUBFRAME_LEN; j += IMS100_MESSAGE_LEN) {
			offset = i + j;

			/* Copy first message */
			bitcpy(staging, src, offset, IMS100_SUBFRAME_VALUELEN);

			/* Check parity */
			validmask = validmask << 1 | (count_ones(staging, 2) != staging[2] ? 1 : 0);

			/* Copy data bits */
			*dst++ = staging[0];
			*dst++ = staging[1];

			/* Copy second message */
			bitcpy(staging, src, offset + IMS100_SUBFRAME_VALUELEN, IMS100_SUBFRAME_VALUELEN);

			/* Check parity */
			validmask = validmask << 1 | (count_ones(staging, 2) != staging[2] ? 1 : 0);

			/* Copy data bits */
			*dst++ = staging[0];
			*dst++ = staging[1];

		}
	}

	return validmask;
}
