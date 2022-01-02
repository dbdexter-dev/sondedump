#ifndef rs92_protocol_h
#define rs92_protocol_h

#include <stdint.h>

#define RS92_BAUDRATE 4800
#define RS92_FRAME_LEN 4800

/* 4800 bits -> 2400 bits (manchester) = 300 bytes -> 300*8/10 bytes (w/o start & stop bits) */
#define RS92_BARE_FRAME_LEN (300 * 8 / 10)
#define RS92_SUBFRAME_MAX_LEN RS92_BARE_FRAME_LEN

#define RS92_SYNCWORD 0x6999a6999a6aa6a6
#define RS92_SYNC_LEN 8
#define RS92_DATA_LEN 210
#define RS92_RS_LEN 24

/* Reed-Solomon ECC parameters */
#define RS92_REEDSOLOMON_N 255
#define RS92_REEDSOLOMON_K 231
#define RS92_REEDSOLOMON_T (RS92_REEDSOLOMON_N - RS92_REEDSOLOMON_K)
#define RS92_REEDSOLOMON_POLY 0x11D
#define RS92_REEDSOLOMON_FIRST_ROOT 0
#define RS92_REEDSOLOMON_ROOT_SKIP 1


#define RS92_SFTYPE_INFO 0x65
#define RS92_SFTYPE_PTU 0x69
#define RS92_SFTYPE_GPSRAW 0x67

#define RS92_CALIB_FRAGSIZE 16
#define RS92_CALIB_FRAGCOUNT 0x20


#define RS92_SERIAL_LEN 10

typedef struct {
	uint8_t syncword[3];
	uint8_t data[RS92_DATA_LEN];
	uint8_t rs_checksum[RS92_RS_LEN];
} __attribute__((packed)) RS92Frame;

typedef struct {
	uint8_t type;
	uint8_t len;    /* In words (2x8 bits), data section only (CRC not included) */
	uint8_t data[RS92_SUBFRAME_MAX_LEN];
} __attribute__((packed)) RS92Subframe;

/* Specific subframe types {{{ */
typedef struct {
	uint8_t type;
	uint8_t len;

	uint16_t seq;
	char serial[RS92_SERIAL_LEN];
	uint8_t _unknown[3];

	uint8_t frag_seq;
	uint8_t frag_data[16];
	uint16_t crc;
} __attribute__((packed)) RS92Subframe_Info;

typedef struct {
	uint8_t type;
	uint8_t len;

	uint32_t ms;
	uint8_t _pad0[10];

	uint8_t sat_info[12];
	struct {
		int32_t p_range;
		uint8_t p_range2[3];
		uint8_t snr;
	} sat_data[12];

	uint16_t crc;
} __attribute__((packed)) RS92Subframe_GPSRaw;
/* }}} */

typedef struct {
	uint8_t data[RS92_CALIB_FRAGSIZE * RS92_CALIB_FRAGCOUNT];
} __attribute__((packed)) RS92Calibration;

#endif
