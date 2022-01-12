#ifndef bitops_h
#define bitops_h

#include <stdint.h>
#include <stdlib.h>

/**
 * Copy bits from one place to the other. Starting bit may or may not be
 * byte-aligned.
 *
 * @param dst buffer to write bits to
 * @param src buffer to read bits from
 * @param src_offset offset of the first bit to copy
 * @param num_bits number of bits to copy
 */
void bitcpy(uint8_t *dst, const uint8_t *src, size_t src_offset, size_t num_bits);

/**
 * Merge contiguous bits into a number
 *
 * @param data pointer to raw data bytes
 * @param nbits number of bits to merge
 * @return merged bits
 */
uint64_t bitmerge(const uint8_t *data, int nbits);

/**
 * Pack loose bits together, with an optional offset
 *
 * @param dst buffer to write bits to
 * @param src buffer to read bits from
 * @param bit_offset dst offset to start writing bits from
 * @param nbits number of bits to write
 */
void bitpack(uint8_t *dst, const uint8_t *src, int bit_offset, int nbits);

/**
 * Clear bits within a buffer
 *
 * @param dst buffer to clear bits from
 * @param bit_offset offset from the start of dst to write zeros from, in bits
 * @param nbits number of bits to clear
 */
void bitclear(uint8_t *dst, int bit_offset, int nbits);

/**
 * Count the number of bits set within a byte array
 *
 * @param data input array
 * @param len length of the input arary, in bytes
 * @return number of bits set in the array
 */
int count_ones(const uint8_t *data, size_t len);

/**
 * Convert 4 bytes into a IEEE754 float
 */
float ieee754_be(const uint8_t *raw);

#endif
