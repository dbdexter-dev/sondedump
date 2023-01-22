#ifndef mrzn1_protocol_h
#define mrzn1_protocol_h

#include <stdint.h>
#include "utils.h"

#define MRZN1_BAUDRATE 2400
#define MRZN1_SYNCWORD 0x666666666555a599
#define MRZN1_SYNC_LEN 8

#define MRZN1_FRAME_LEN 816

#define MRZN1_CALIB_FRAGCOUNT   16
#define MRZN1_CALIB_FRAGSIZE    4

PACK(typedef struct {
	uint8_t data[MRZN1_FRAME_LEN/8];
}) MRZN1RawFrame;

PACK(typedef struct {
	uint8_t sync[4];
	uint8_t seq;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t ms;

	int32_t x;
	int32_t y;
	int32_t z;
	int16_t dx;
	int16_t dy;
	int16_t dz;
	uint8_t num_sats;

	uint8_t _unk0[2];
	int16_t temp;
	int16_t rh;

	uint8_t _unk1[2];
	int32_t _unk2;
	int32_t _unk3;

	uint8_t calib_frag_seq;
	uint8_t calib_data[4];

	uint8_t crc[2];
}) MRZN1Frame;

PACK(typedef struct {
	float coeffs[9];
	uint32_t serials[7];
}) MRZN1Calibration;

#endif
