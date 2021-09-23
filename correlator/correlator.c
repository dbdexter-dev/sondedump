#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "correlator.h"

static inline int inverse_correlate_u64(uint64_t x, uint64_t y);

void
correlator_init(Correlator *c, uint64_t syncword, int sync_len)
{
	c->syncword = syncword;
	c->sync_len = sync_len;
}


int
correlate(Correlator *c, int *inverted, uint8_t *restrict hard_frame, int len)
{
	const uint64_t syncword = c->syncword;
	const uint64_t inverse_syncword = syncword ^ ~(uint64_t)0;
	const int sync_len = c->sync_len;
	int corr, best_corr, best_offset;
	int i, j;
	uint64_t window;
	uint8_t tmp;

	best_corr = 8*c->sync_len;
	best_offset = 0;

	/* Initalize window with the first n bytes in the frame */
	window = 0;
	for (i=0; i<sync_len; i++) {
		window = (uint64_t)*hard_frame << (8*(7-i));
		hard_frame++;
	}

	/* If the syncword is found at offset 0, we're already sync'd up: return */
	if (inverse_correlate_u64(syncword, window) == 0) {
		return 0;
	}

	/* For each byte in the frame */
	for (i=0; i<len-sync_len; i++) {
		/* Fetch a byte from the frame */
		tmp = *hard_frame++;

		/* For each bit in the byte */
		for (j=0; j<8; j++) {

			/* Check correlation */
			corr = inverse_correlate_u64(syncword, window);
			if (corr < best_corr) {
				best_corr = corr;
				best_offset = i*8 + j;
				*inverted = 0;
			}

			/* Check correlation for the inverted syncword */
			corr = inverse_correlate_u64(inverse_syncword, window);
			if (corr < best_corr) {
				best_corr = corr;
				best_offset = i*8 + j;
				*inverted = 1;
			}

			/* Advance window by one */
			window = ((window << 1) | ((tmp >> (7-j)) & 0x1));
		}
	}

	return best_offset;
}


/* Static functions {{{ */
/**
 * Count the number of bits that differ between two uint64's
 */
static inline int
inverse_correlate_u64(uint64_t x, uint64_t y)
{
	int corr;
	uint64_t v = x ^ y;

	for (corr = 0; v; corr++) {
		v &= v-1;
	}

	return corr;
}
/* }}} */
