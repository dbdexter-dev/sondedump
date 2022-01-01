#ifndef manchester_h
#define manchester_h

#include <stdint.h>

void manchester_decode(uint8_t *dst, uint8_t *src, int nbits);

#endif
