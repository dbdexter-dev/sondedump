#include "crc.h"

uint16_t
crc16_ccitt_false(const void *v_data, size_t len)
{
	const uint8_t *data = v_data;
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
crc16_aug_ccitt(const void *v_data, size_t len)
{
	const uint8_t *data = v_data;
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

uint16_t
fcs16(const void *v_data, size_t len)
{
	const uint8_t *data = v_data;
	uint8_t sum[2] = {0, 0};

	for (; len>0; len--) {
		sum[0] += *data++;
		sum[1] += sum[0];
	}

	return sum[0] << 8 | sum[1];
}
