#include "bitops.h"
#include "frame.h"

void
imet4_frame_descramble(IMET4Frame *frame)
{
	int i;
	uint8_t *raw_frame;
	uint8_t byte, tmp;

	raw_frame = frame->data;

	/* Reorder bits in the frame and remove start/stop bits */
	for (i=0; i < (int)sizeof(frame->data); i++) {

		bitcpy(&byte, raw_frame, i * 10 + 1, 8);

		tmp = 0;
		for (int j=0; j<8; j++) {
			tmp |= ((byte >> (7-j)) & 0x1) << j;
		}

		raw_frame[i] = tmp;
	}
}
