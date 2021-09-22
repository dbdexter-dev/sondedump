#ifndef gfsk_h
#define gfsk_h

#include <stdint.h>
#include <stdlib.h>

#define GFSK_FILTER_ORDER 64
#define SYM_BW 0.00005

/**
 * Initialize the GFSK decoder
 *
 * @param samplerate expected input sample rate
 * @param symrate expected output symbol rate
 */
int gfsk_init(int samplerate, int symrate);

void gfsk_deinit();

/**
 * Decode the given GFSK-coded samples into bits
 *
 * @param dst destination buffer where the bits will be written to
 * @param bit_offset offset from the start of dst where bits should be written, in bits
 * @param len number of bits to decode
 * @param read pointer to function used to read samples
 *
 * @return offset of the last bit decoded
 */
int gfsk_decode(uint8_t *dst, int bit_offset, size_t len, int (*read)(float *dst, size_t len));

#endif
