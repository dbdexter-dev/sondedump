#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bitops.h"

void
bitcpy(uint8_t *dst, const uint8_t *src, size_t offset, size_t bits)
{
	src += offset / 8;
	offset %= 8;

	/* All but last reads */
	for (; bits > 8; bits -= 8) {
		*dst    = *src++ << offset;
		*dst++ |= *src >> (8 - offset);
	}

	/* Last read */
	if (offset + bits < 8) {
		*dst = (*src << offset) & ~((1 << (8 - bits)) - 1);
	} else {
		*dst  = *src++ << offset;
		*dst |= *src >> (8 - offset);
		*dst &= ~((1 << (8-bits)) - 1);
	}
}

uint64_t
bitmerge(const uint8_t *data, int nbits)
{
	uint64_t ret = 0;

	for (; nbits >= 8; nbits-=8) {
		ret = (ret << 8) | *data++;
	}

	return (ret << nbits) | (*data >> (7 - nbits));
}

void
bitpack(uint8_t *dst, const uint8_t *src, int bit_offset, int nbits)
{
	uint8_t tmp;

	dst += bit_offset/8;
	bit_offset %= 8;

	tmp = *dst >> (8 - bit_offset);
	for (; nbits > 0; nbits--) {
		tmp = (tmp << 1) | *src++;
		bit_offset++;

		if (!(bit_offset % 8)) {
			*dst++ = tmp;
			tmp = 0;
		}
	}

	if (bit_offset % 8) {
		*dst &= (1 << (8 - bit_offset%8)) - 1;
		*dst |= tmp << (8 - bit_offset%8);
	}
}

void
bitclear(uint8_t *dst, int bit_offset, int nbits)
{
	dst += bit_offset/8;
	bit_offset %= 8;

	/* Special case where start and end might be in the same byte */
	if (bit_offset + nbits < 8) {
		*dst &= ~((1 << (8 - bit_offset)) - 1)
		        | ((1 << (8 - bit_offset - nbits)) - 1);
		return;
	}

	if (bit_offset) {
		*dst &= ~((1 << (8 - bit_offset)) - 1);
		dst++;
		nbits -= (8 - bit_offset);
	}

	memset(dst, 0, nbits/8);
	dst += nbits/8;

	if (nbits % 8) {
		*dst &= (1 << (8 - nbits % 8)) - 1;
	}
}

int
count_ones(const uint8_t *data, size_t len)
{
	uint8_t tmp;
	int count = 0;

	for (; len>0; len--) {
		tmp = *data++;
		for (; tmp; count++) {
			tmp &= tmp-1;
		}
	}

	return count;
}

float
ieee754_be(const uint8_t *raw)
{
	union {
		uint32_t raw;
		float value;
	} data;

	data.raw = raw[0] << 24 | raw[1] << 16 | raw[2] << 8 | raw[3];
	return data.value;
}

