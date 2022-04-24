#include "frame.h"

void
imet4_frame_descramble(IMET4Frame *frame)
{
	int i;
	uint8_t *raw_frame;

	raw_frame = (uint8_t*)frame;

	/* Reorder bits in the frame and descramble */
	for (i=0; i < (int)sizeof(*frame); i++) {
		uint8_t tmp = 0;
		for (int j=0; j<8; j++) {
			tmp |= ((raw_frame[i] >> (7-j)) & 0x1) << j;
		}
		raw_frame[i] = tmp;
	}
}
