#ifndef c50_protocol_h
#define c50_protocol_h

#include <stdint.h>
#include "utils.h"

#define C50_BAUDRATE 2400
#define C50_SYNCWORD 0x005FF
#define C50_SYNC_LEN 20
#define C50_MARK_FREQ 4700.0
#define C50_SPACE_FREQ 2900.0

#define C50_FRAME_LEN 90

enum c50_type {
	C50_TYPE_TEMP_REF = 0x02,
	C50_TYPE_TEMP_AIR = 0x03,
	C50_TYPE_TEMP_HUM = 0x04,
	C50_TYPE_TEMP_TOP = 0x05,
	C50_TYPE_TEMP_O3_INTAKE = 0x06,

	C50_TYPE_RH = 0x11,

	C50_TYPE_DATE = 0x14,
	C50_TYPE_TIME = 0x15,
	C50_TYPE_LAT = 0x16,
	C50_TYPE_LON = 0x17,
	C50_TYPE_ALT = 0x18,

	C50_TYPE_SN = 0x64,
};

PACK(typedef struct {
	uint8_t data[C50_FRAME_LEN/8];
}) C50RawFrame;

PACK(typedef struct {
	uint8_t sync[2];
	uint8_t type;
	uint8_t data[4];
	uint8_t checksum[2];
}) C50Frame;

#endif
