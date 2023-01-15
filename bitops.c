#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "bitops.h"

void
bitcpy(void *v_dst, const void *v_src, size_t offset, size_t bits)
{
	uint8_t *dst = v_dst;
	const uint8_t *src = v_src;

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
bitmerge(const void *v_data, int nbits)
{
	const uint8_t *data = v_data;
	uint64_t ret = 0;

	for (; nbits >= 8; nbits-=8) {
		ret = (ret << 8) | *data++;
	}

	return (ret << nbits) | (*data >> (7 - nbits));
}

void
bitpack(void *v_dst, const void *v_src, int bit_offset, int nbits)
{
	const uint8_t *src = v_src;
	uint8_t *dst = v_dst;

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
bitclear(void *v_dst, int bit_offset, int nbits)
{
	uint8_t *dst = v_dst;

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
count_ones(const void *v_data, size_t len)
{
	const uint8_t *data = v_data;
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
ieee754_be(const void *v_raw)
{
	const uint8_t *raw = v_raw;
	union {
		uint32_t raw;
		float value;
	} data;

	data.raw = raw[0] << 24 | raw[1] << 16 | raw[2] << 8 | raw[3];
	return data.value;
}

float
mbf_le(const void *v_raw)
{
	const uint8_t *raw = v_raw;
	uint8_t sign, exp;
	uint32_t mantissa;
	uint32_t ieee;

	exp = raw[3];
	sign = raw[2] >> 7;
	mantissa = (raw[2] & 0x7F) << 16 | raw[1] << 8 | raw[0];

	ieee = sign << 31
		 | (exp - 2) << 23
		 | mantissa;

	return *(float*)&ieee;
}

