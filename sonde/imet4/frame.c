#include "bitops.h"
#include "frame.h"

void
imet4_frame_descramble(IMET4Frame *dst, IMET4Frame *src)
{
	int i;
	uint8_t *raw_src = (uint8_t*)src->data;
	uint8_t *raw_dst = (uint8_t*)dst->data;

	uint8_t byte, tmp;

	/* Reorder bits in the frame and remove start/stop bits */
	for (i=0; i < IMET4_FRAME_LEN/8; i++) {

		bitcpy(&byte, raw_src, i * 10 + 1, 8);

		tmp = 0;
		for (int j=0; j<8; j++) {
			tmp |= ((byte >> (7-j)) & 0x1) << j;
		}

		raw_dst[i] = tmp;
	}
}
