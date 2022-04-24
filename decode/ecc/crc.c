#include "crc.h"

uint16_t
crc16_ccitt_false(uint8_t *data, size_t len)
{
	int i;
	uint16_t crc;

	crc = CCITT_FALSE_INIT;

	for(; len > 0; len--) {
		crc ^= *data++ << 8;
		for (i=0; i<8; i++) {
			crc = crc & 0x8000 ? (crc << 1) ^ CCITT_FALSE_POLY : (crc << 1);
		}
	}

	return crc;
}

uint16_t
crc16_aug_ccitt(uint8_t *data, size_t len)
{
	int i;
	uint16_t crc;

	crc = AUG_CCITT_INIT;

	for(; len > 0; len--) {
		crc ^= *data++ << 8;
		for (i=0; i<8; i++) {
			crc = crc & 0x8000 ? (crc << 1) ^ CCITT_FALSE_POLY : (crc << 1);
		}
	}

	return crc;
}
