#include "prng.h"

void
prng_generate(uint8_t *dst, size_t len, const unsigned *poly, size_t poly_len)
{
	unsigned new_bit;
	uint64_t state;
	uint8_t byte;
	int bit_idx;
	size_t i;

	bit_idx = 0;
	byte = 0;
	state = (1 << poly[0]) - 1;         /* Initialize state to ones */
	while (len) {
		/* Generate bit */
		new_bit = 0;
		for (i=1; i<poly_len; i++) {
			new_bit ^= (state >> poly[i]) & 0x1;
		}

		/* Update output and state */
		byte = (byte << 1) | new_bit;
		state = (state >> 1) | (new_bit << (poly[0]-1));
		bit_idx++;

		if (bit_idx == 8) {
			/* Write output to dst */
			*dst++ = byte;
			len--;
			bit_idx = 0;
			byte = 0;
		}
	}

}
