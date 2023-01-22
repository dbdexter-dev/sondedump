#include "frame.h"
#include "decode/ecc/crc.h"
#include "log/log.h"

int
mrzn1_frame_correct(MRZN1Frame *frame)
{
	uint16_t crc = crc16_modbus(&frame->data, sizeof(*frame) - 4 - 2);
	uint16_t expected = frame->crc[0] | frame->crc[1] << 8;

	if (crc == expected) return 0;
	return -1;
}
