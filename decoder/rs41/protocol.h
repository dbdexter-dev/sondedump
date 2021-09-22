#ifndef rs41_protocol_h
#define rs41_protocol_h

#include <stdint.h>

#define RS41_SYNCWORD 0x086d53884469481f
#define RS41_SYNCLEN 8
#define RS41_FRAME_LEN 320
#define RS41_BAUDRATE 4800

typedef struct {
	uint64_t syncword;
} __attribute__((packed)) RS41Frame;

#endif
