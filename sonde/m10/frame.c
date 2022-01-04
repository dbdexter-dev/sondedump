#include <stdint.h>
#include "frame.h"

static uint16_t m10_crc_step(uint16_t state, uint8_t data);

void
m10_frame_descramble(M10Frame *frame)
{
	uint8_t *raw_frame = (uint8_t*)frame;
	uint8_t tmp, topbit;
	int i;

	topbit = 0;
	for (i=0; i<(int)sizeof(*frame); i++) {
		tmp = raw_frame[i] << 7;
		raw_frame[i] ^= 0xFF ^ (topbit | raw_frame[i] >> 1);
		topbit = tmp;
	}
}

int
m10_frame_correct(M10Frame *frame)
{
	uint8_t *raw_frame = (uint8_t*)frame;
	const uint16_t expected = frame->crc[0] << 8 | frame->crc[1];
	uint16_t crc;
	int i;

	crc = 0;
	for (i=2; i < (int)sizeof(*frame) - 2; i++) {
		crc = m10_crc_step(crc, raw_frame[i]);
	}

	return (crc == expected) ? 0 : -1;
}

static uint16_t
m10_crc_step(uint16_t c, uint8_t b)
{
	int c0, c1, t, t6, t7, s;
	c1 = c & 0xFF;
	// B
	b  = (b >> 1) | ((b & 1) << 7);
	b ^= (b >> 2) & 0xFF;
	// A1
	t6 = ( c     & 1) ^ ((c >> 2) & 1) ^ ((c >> 4) & 1);
	t7 = ((c >> 1) & 1) ^ ((c >> 3) & 1) ^ ((c >> 5) & 1);
	t = (c & 0x3F) | (t6 << 6) | (t7 << 7);
	// A2
	s  = (c >> 7) & 0xFF;
	s ^= (s >> 2) & 0xFF;
	c0 = b ^ t ^ s;
	return ((c1 << 8) | c0) & 0xFFFF;
	return 0;
}
