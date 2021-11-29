#ifndef m10_protocol_h
#define m10_protocol_h

#include <stdint.h>

/* Physical parameters */
#define M10_BAUDRATE 9600

/* Frame parameters */
#define M10_FRAME_LEN 1024

#define M10_SYNCWORD 0x999999ac
#define M10_SYNCLEN 4

typedef struct {
	uint8_t data[M10_FRAME_LEN];
} __attribute__((packed)) M10Frame;

#endif
