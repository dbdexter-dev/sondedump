#ifndef prng_h
#define prng_h

#include <stdint.h>
#include <stdlib.h>

void prng_generate(uint8_t *dst, size_t len, const unsigned *poly, size_t poly_len);

#endif
