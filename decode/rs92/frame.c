#include <string.h>
#include "frame.h"
#include "utils.h"

void
rs92_frame_descramble(RS92Frame *frame)
{
	int i;
	uint8_t *in, *out;

	in = out = (uint8_t*)frame;

	for (i=0; i<RS92_BARE_FRAME_LEN; i++) {
		/* Remove start and stop bits */
		bitcpy(out, in, 10*i+2, 8);

		/* Reverse bits within the byte */
		*out = ((*out * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32;
		out++;
	}
}

int
rs92_frame_correct(RS92Frame *frame, RSDecoder *rs)
{
	int errors;
	uint8_t rs_block[RS92_REEDSOLOMON_N];

	/* Pad block to 255 bytes */
	memcpy(rs_block, frame->data, sizeof(frame->data));
	memset(rs_block + sizeof(frame->data), 0, RS92_REEDSOLOMON_K - sizeof(frame->data));
	memcpy(rs_block + RS92_REEDSOLOMON_K, frame->rs_checksum, sizeof(frame->rs_checksum));

	/* Error correct */
	errors = rs_fix_block(rs, rs_block);

	/* Remove padding */
	memcpy(frame->data, rs_block, sizeof(frame->data));
	memcpy(frame->rs_checksum, rs_block + RS92_REEDSOLOMON_K, sizeof(frame->rs_checksum));

	return errors;
}
