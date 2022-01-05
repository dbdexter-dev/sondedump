#include <string.h>
#include "frame.h"

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
	for (i=0; i < (int)sizeof(*frame); i += IMS100_SUBFRAME_LEN) {
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
		}
	}

	return errcount;
}

