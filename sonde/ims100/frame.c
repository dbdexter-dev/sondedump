#include <math.h>
#include <string.h>
#include <time.h>
#include "bitops.h"
#include "frame.h"
#include "utils.h"
#include "log/log.h"

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

