#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "bitops.h"
#include "correlator.h"
#include "log/log.h"

static inline int inverse_correlate_u64(uint64_t x, uint64_t y);

void
correlator_init(Correlator *c, uint64_t syncword, int sync_len)
{
	c->syncword = syncword;
	c->sync_len = sync_len;
}


int
correlate(Correlator *c, int *inverted, const uint8_t *restrict hard_frame, int len)
{
	const int sync_len = c->sync_len;
	const uint64_t syncmask = (sync_len < 64) ? ((1ULL << (sync_len)) - 1) : ~0ULL;
	const uint64_t syncword = c->syncword & syncmask;
	int corr, best_corr, best_offset;
	int i, j;
	uint64_t window;
	uint8_t tmp;

	best_corr = sync_len;
	best_offset = 0;

	window = 0;

	/* For each byte in the frame */
	for (i=0; i<len+sync_len/8; i++) {
		/* Fetch a byte from the frame */
		tmp = *hard_frame++;

		/* For each bit in the byte */
		for (j=0; j<8; j++) {

			/* If the window is full */
			if (i*8 + j >= sync_len) {

				/* Check correlation */
				corr = inverse_correlate_u64(syncword, window);
				if (corr < best_corr) {
					best_corr = corr;
					best_offset = i*8 + j;
					if (inverted) *inverted = 0;
				}

				/* Check correlation for the inverted syncword */
				corr = sync_len - corr;
				if (corr < best_corr) {
					best_corr = corr;
					best_offset = i*8 + j;
					if (inverted) *inverted = 1;
				}

				if (best_corr == 0) return best_offset - sync_len;
			}

			/* Advance window by one */
			window = ((window << 1) | ((tmp >> (7-j)) & 0x1)) & syncmask;
		}
	}

	return best_offset - sync_len;
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
