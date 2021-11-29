#include <stdint.h>
#include "frame.h"

void
m10_frame_descramble(M10Frame *frame)
{
	uint8_t *raw_frame = (uint8_t*)frame;
	uint8_t tmp, topbit;
	int i;

	for (i=0; i<sizeof(*frame); i++) {
		tmp = raw_frame[i] << 7;
		raw_frame[i] ^= (topbit | (raw_frame[i] >> 1));
		topbit = tmp;
	}
}
