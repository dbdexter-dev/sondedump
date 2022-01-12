#include <stdio.h>
#include "manchester.h"
#include "bitops.h"

void
manchester_decode(uint8_t *dst, uint8_t *src, int nbits)
{
	uint8_t *raw_dst = (uint8_t*)dst;
	uint8_t out;
	uint8_t inBits;
	int i, out_count;

	out = 0;
	out_count = 0;
	for (i=0; i<nbits; i+=2) {
		bitcpy(&inBits, src, i, 2);
		out = (out << 1) | (inBits & 0x40 ? 1 : 0);
		out_count++;

		if (!(out_count % 8)) {
			*raw_dst++ = out;
			out = 0;
		}
	}
	*raw_dst = out;
}
