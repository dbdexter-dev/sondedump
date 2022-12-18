#ifndef c50_protocol_h
#define c50_protocol_h

#include <stdint.h>
#include "utils.h"

#define C50_BAUDRATE 2400
#define C50_SYNCWORD 0x00FF
#define C50_SYNC_LEN 2
#define C50_MARK_FREQ 4700.0
#define C50_SPACE_FREQ 2900.0

#define C50_FRAME_LEN 90

PACK(typedef struct {
	uint8_t data[C50_FRAME_LEN/8];
}) C50Frame;

#endif
