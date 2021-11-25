#ifndef manchester_h
#define manchester_h

#include <stdint.h>

/**
 * Manchester decode
 *
 * @param dst pointer to buffer to write bits to
 * @param src pointer to buffer to read bits from
 * @param offset number of bits to skip in dst
 * @param nbits number of bits to decode
 */
void manchester_decode(uint8_t *dst, const uint8_t *src, int offset, int nbits);

#endif
