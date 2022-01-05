#ifndef ims100_protocol_h
#define ims100_protocol_h

#include <stdint.h>
#include <time.h>
#include "utils.h"

#define IMS100_BAUDRATE 2400

#define IMS100_SYNCWORD 0xaaa56a659a99559a
#define IMS100_SYNC_LEN 8
#define IMS100_FRAME_LEN 1200

#define IMS100_SUBFRAME_LEN 300
#define IMS100_SUBFRAME_BCHLEN 12
#define IMS100_SUBFRAME_VALUELEN 17
#define IMS100_MESSAGE_LEN (2 * IMS100_SUBFRAME_VALUELEN + IMS100_SUBFRAME_BCHLEN)

#define IMS100_REEDSOLOMON_N 63
#define IMS100_REEDSOLOMON_K 51
#define IMS100_REEDSOLOMON_T (IMS100_REEDSOLOMON_N-IMS100_REEDSOLOMON_K)
#define IMS100_REEDSOLOMON_POLY 0x61

extern uint8_t ims100_bch_roots[];

/* Even & odd frame types {{{ */
typedef struct {
	uint8_t seq[2];
	uint8_t data[2 * 9];
	uint8_t ms[2];
	uint8_t hour;
	uint8_t min;

	uint8_t data2[2 * 12];

	uint32_t valid;
} __attribute__((packed)) IMS100FrameEven;

typedef struct {
} __attribute__((packed)) IMS100FrameOdd;
/* }}} */

typedef struct {
	uint8_t syncword[3];
	uint8_t seq[2];

	uint8_t data[70];
} __attribute__((packed)) IMS100Frame;

inline uint16_t IMS100Frame_seq(const IMS100Frame *frame) {
	return (uint16_t)frame->seq[0] << 8 | frame->seq[1];
}

inline uint16_t IMS100FrameEven_ms(const IMS100FrameEven *frame) {
	return (uint16_t)frame->ms[0] << 8 | frame->ms[1];
}

inline uint8_t IMS100FrameEven_hour(const IMS100FrameEven *frame) {
	return frame->hour;
}

inline uint8_t IMS100FrameEven_min(const IMS100FrameEven *frame) {
	return frame->min;
}

inline time_t IMS100FrameEven_time(const IMS100FrameEven *frame) {
	if ((frame->valid & 0xF000) != 0xF000) return 0;
	return (time_t)IMS100FrameEven_ms(frame) / 1000
	     + (time_t)IMS100FrameEven_min(frame) * 60
	     + (time_t)IMS100FrameEven_hour(frame) * 3600;
}

#endif
