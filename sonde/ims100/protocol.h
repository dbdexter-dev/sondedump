#ifndef ims100_protocol_h
#define ims100_protocol_h

#include <stdint.h>

#define IMS100_BAUDRATE 2400

#define IMS100_SYNCWORD 0xaaa56a659a99559a
#define IMS100_SYNC_LEN 8
#define IMS100_FRAME_LEN 1200

typedef struct {
	uint8_t syncword[3];
	uint8_t raw[72];
} __attribute__((packed)) IMS100Frame;

#endif
