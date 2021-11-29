#include <stdio.h>
#include "manchester.h"
#include "utils.h"

void
manchester_decode(uint8_t *dst, uint8_t *src, int offset, int nbits)
{
	int i;
	uint8_t tmp, out;

	dst += offset/8;
	offset %= 8;

	out = (*dst & ~((1 << offset) - 1)) >> offset;
	tmp = 0;

	for (i=0; nbits > 0; i+=2, nbits--) {
		tmp = 0;
		bitcpy(&tmp, src, i, 2);

		/* 0x40 = low-high, 0x80 = high-low, others are invalid */
		out <<= 1;
		if (tmp == 0x40) out |= 1;

		offset++;
		if (!(offset % 8)) {
			*dst++ = out;
			out = 0;
		}
	}

	if (offset % 8) {
		*dst = (out << (8 - offset));
	}
}
