#include "crc.h"

#define CCITT_FALSE_POLY 0x1021
#define CCITT_FALSE_INIT 0xFFFF
#define AUG_CCITT_POLY   0x1021
#define AUG_CCITT_INIT   0x1D0F
#define MODBUS_POLY      0xA001
#define MODBUS_INIT      0xFFFF

static uint16_t crc16(uint16_t init, uint16_t poly, const void *data, size_t len);
static uint16_t crc16_refin_refout(uint16_t init, uint16_t poly, const void *vdata, size_t len);

uint16_t
crc16_ccitt_false(const void *data, size_t len)
{
	return crc16(CCITT_FALSE_INIT, CCITT_FALSE_POLY, data, len);
}

uint16_t
crc16_aug_ccitt(const void *data, size_t len)
{
	return crc16(AUG_CCITT_INIT, AUG_CCITT_POLY, data, len);
}

uint16_t
crc16_modbus(const void *data, size_t len)
{
	return crc16_refin_refout(MODBUS_INIT, MODBUS_POLY, data, len);
}

static uint16_t
crc16(uint16_t init, uint16_t poly, const void *vdata, size_t len)
{
	const uint8_t *data = vdata;
	uint16_t crc = init;
	int i;

	for(; len > 0; len--) {
		crc ^= *data++ << 8;
		for (i=0; i<8; i++) {
			crc = crc & 0x8000 ? (crc << 1) ^ poly : (crc << 1);
		}
	}

	return crc;
}

static uint16_t
crc16_refin_refout(uint16_t init, uint16_t poly, const void *vdata, size_t len)
{
	const uint8_t *data = vdata;
	uint16_t crc = init;
	int i;

	for(; len > 0; len--) {
		crc ^= *data++;
		for (i=0; i<8; i++) {
			crc = crc & 0x0001 ? (crc >> 1) ^ poly : (crc >> 1);
		}
	}

	return crc;
}
