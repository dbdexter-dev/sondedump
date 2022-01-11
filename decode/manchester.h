#ifndef manchester_h
#define manchester_h

#include <stdint.h>

/**
 * Manchester decode bits
 *
 * @param dst destination buffer to write bits to
 * @param src source buffer to read bit pairs from
 * @param nbits number of bits to decode (= output bits)
 */
void manchester_decode(uint8_t *dst, uint8_t *src, int nbits);

#endif
