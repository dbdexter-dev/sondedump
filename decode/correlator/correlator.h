#ifndef correlator_h
#define correlator_h

#include <stdint.h>
#include "utils.h"

typedef struct {
	uint64_t syncword;
	int sync_len;
} Correlator;

/**
 * Initialize the correlator with a synchronization sequence
 *
 * @param syncword synchronization word
 * @param len synchronization word length
 */
void correlator_init(Correlator *c, uint64_t syncword, int sync_len);

/**
 * Look for the synchronization word inside a buffer
 *
 * @param hard pointer to a byte buffer containing the data to correlate
 *        the syncword to
 * @param len length of the byte buffer, in bytes
 * @return the offset with the highest correlation to the syncword
 */
int  correlate(Correlator *c, int *inverted, const uint8_t *hard, int len);

#endif
