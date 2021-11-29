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
