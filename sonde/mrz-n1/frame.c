#include "frame.h"
#include "decode/ecc/crc.h"
#include "log/log.h"

int
mrzn1_frame_correct(MRZN1Frame *frame)
{
	uint8_t *crc_ptr;
	size_t crc_len;
	uint16_t expected = frame->crc[0] | frame->crc[1] << 8;
	uint16_t crc;

	/* CRC covers from the sequence number to the end of the packet */
	crc_ptr = ((uint8_t*)frame) + sizeof(frame->sync);
	crc_len = sizeof(*frame) - sizeof(frame->sync) - sizeof(frame->crc);

	crc = crc16_modbus(crc_ptr, crc_len);

	if (crc == expected) return 0;
	return -1;
}
