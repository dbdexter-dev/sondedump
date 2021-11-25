#ifndef rs92_protocol_h
#define rs92_protocol_h

#include <stdint.h>

#define RS92_BAUDRATE 4800
#define RS92_FRAME_LEN (RS92_BAUDRATE/8)
#define RS92_DECODED_FRAME_LEN (RS92_FRAME_LEN/2)


typedef struct {
	uint8_t data[RS92_FRAME_LEN];
} __attribute__((packed)) RS92Frame;

#endif
