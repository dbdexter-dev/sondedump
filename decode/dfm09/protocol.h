#ifndef dfm09_protocol_h
#define dfm09_protocol_h

#include <stdint.h>

#define DFM09_BAUDRATE 2500
#define DFM09_FRAME_LEN (DFM09_BAUDRATE/8)

#define DFM09_SYNCWORD 0x6aa6a666966aa6a6
#define DFM09_SYNC_LEN 8

typedef struct {
	uint8_t data[DFM09_FRAME_LEN];
} __attribute__((packed)) DFM09Frame;

#endif
