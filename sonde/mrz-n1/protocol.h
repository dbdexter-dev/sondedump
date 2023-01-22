#ifndef mrzn1_protocol_h
#define mrzn1_protocol_h

#include <stdint.h>
#include "utils.h"

#define MRZN1_BAUDRATE 2400
#define MRZN1_SYNCWORD 0x666666666555a599
#define MRZN1_SYNC_LEN 8

#define MRZN1_FRAME_LEN 816

PACK(typedef struct {
	uint8_t data[MRZN1_FRAME_LEN/8];
}) MRZN1RawFrame;

PACK(typedef struct {
	uint8_t sync[4];
	uint8_t data[MRZN1_FRAME_LEN/8/2 - 4 - 2];
	uint8_t crc[2];
}) MRZN1Frame;

#endif
