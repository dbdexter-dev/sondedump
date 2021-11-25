#include "manchester.h"

void
manchester_decode(uint8_t *dst, const uint8_t *src, int offset, int nbits)
{
	uint8_t in, out;
	int i;

	dst += offset/8;
	offset %= 8;

	out = (*dst & ~((1 << offset) - 1)) >> offset;
	for (i=0; i<2*nbits; i+=2) {
		in = (src[i/8] >> (i%8)) & 0x03;

		switch (in) {
			case 0x01:  /* Low-high transition: 0 bit */
				out = (out << 1) | 0;
				offset++;
				break;
			case 0x02:  /* High-low transition: 1 bit */
				out = (out << 1) | 1;
				offset++;
				break;
			case 0x00:
			case 0x03:  /* No transition: misaligned */
				offset++;
				break;
		}

		if (!(offset%8)) {
			*dst++ = out;
			out = 0;
		}
	}
}
