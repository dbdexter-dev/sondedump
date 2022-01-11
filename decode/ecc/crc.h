#ifndef crc_h
#define crc_h

#include <stdint.h>
#include <stdlib.h>

#define CCITT_FALSE_POLY 0x1021
#define CCITT_FALSE_INIT 0xFFFF

/**
 * Compute a CRC16 on data using the CCITT-FALSE parameters
 *
 * @param data pointer to the data to compute the CRC on
 * @param len data length
 * @return the resulting CRC value
 */
uint16_t crc16_ccitt_false(uint8_t *data, size_t len);

#endif
