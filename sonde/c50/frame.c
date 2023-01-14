#include <stddef.h>
#include "bitops.h"
#include "frame.h"
#include "protocol.h"
#include "decode/ecc/crc.h"
#include "log/log.h"

void
c50_frame_descramble(C50Frame *dst, const C50RawFrame *src)
{
	uint8_t *raw_src = (uint8_t*)src;
	uint8_t *raw_dst = (uint8_t*)dst;
	uint8_t byte;
	int i, j;

	for (i=0; i<C50_FRAME_LEN/10; i++) {
		bitcpy(&byte, raw_src, 10*i + 1, 8);

		for (j=0; j<8; j++) {
			raw_dst[i] = (raw_dst[i]) << 1 | (byte & 0x1);
			byte >>= 1;
		}
	}
}

int
c50_frame_correct(C50Frame *frame)
{
	const uint16_t expected = frame->checksum[0] << 8 | (frame->checksum[1] ^ 0xFF);
	uint16_t checksum;

	checksum = fcs16(&frame->type, sizeof(frame->type) + sizeof(frame->data));

	if (checksum != expected) {
		return -1;
	}

	return 0;
}
