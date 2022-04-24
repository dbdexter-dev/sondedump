#ifndef imet4_protocol_h
#define imet4_protocol_h

#include <stdint.h>
#include "utils.h"

#define IMET4_BAUDRATE 1200
#define IMET4_SYNCWORD 0xFFFFFF40
#define IMET4_SYNC_LEN 4
#define IMET4_MARK_FREQ 2200.0
#define IMET4_SPACE_FREQ 1200.0

#define IMET4_FRAME_LEN 600

PACK(typedef struct {
	uint8_t sync[3];
	uint8_t data[IMET4_FRAME_LEN/8 - 3];
}) IMET4Frame;

#endif
