#include "framer.h"
#include "utils.h"

int
read_frame_gfsk(GFSKDemod *gfsk, Correlator *corr, uint8_t *dst, int (*read)(float *dst), size_t framelen, size_t bit_offset)
{
	int i, offset, inverted;

	/* First read */
	if (!gfsk_demod(gfsk, dst, bit_offset, framelen - bit_offset, read)) return -1;

	/* Find sync marker */
	offset = correlate(corr, &inverted, dst, framelen/8);
	if (offset) {
		if (!gfsk_demod(gfsk, dst, framelen, offset, read)) return -1;
		bitcpy(dst, dst, offset, framelen);
	}

	/* Invert if necessary */
	if (inverted) {
		for (i=0; i < (int)framelen/8; i++) {
			dst[i] ^= 0xFF;
		}
	}

	return offset;
}
