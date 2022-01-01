#ifndef rs92_protocol_h
#define rs92_protocol_h

#include <stdint.h>

#define RS92_BAUDRATE 4800
#define RS92_FRAME_LEN 4800
#define RS92_BARE_FRAME_LEN (RS92_FRAME_LEN * 8 / 10)

#define RS92_SYNCWORD 0x6aa6a666966aa6a6
#define RS92_SYNC_LEN 8

typedef struct {
	uint8_t data[300];
} __attribute__((packed)) RS92Frame;

#endif
