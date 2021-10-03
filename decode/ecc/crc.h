#ifndef crc_h
#define crc_h

#include <stdint.h>
#include <stdlib.h>

#define CCITT_FALSE_POLY 0x1021
#define CCITT_FALSE_INIT 0xFFFF

uint16_t crc16_ccitt_false(uint8_t *data, size_t len);

#endif
