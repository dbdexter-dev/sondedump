#ifndef rs92_protocol_h
#define rs92_protocol_h

#include <stdint.h>

#define RS92_BAUDRATE 4800
#define RS92_FRAME_LEN 300

#define RS92_SYNCWORD 0xa6999a6999a6999a
#define RS92_SYNC_LEN 8

typedef struct {
	uint8_t data[RS92_FRAME_LEN];
} __attribute__((packed)) RS92Frame;

#endif
