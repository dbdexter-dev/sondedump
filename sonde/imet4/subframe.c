#include "decode/ecc/crc.h"
#include "subframe.h"

size_t
imet4_subframe_len(IMET4Subframe *sf)
{
	switch (sf->type) {
		case IMET4_SFTYPE_PTU:
			return sizeof(IMET4Subframe_PTU) + 2;
		case IMET4_SFTYPE_GPS:
			return sizeof(IMET4Subframe_GPS) + 2;
		case IMET4_SFTYPE_XDATA:
			return sf->data[0] + 4;
		case IMET4_SFTYPE_PTUX:
			return sizeof(IMET4Subframe_PTUX) + 2;
		case IMET4_SFTYPE_GPSX:
			return sizeof(IMET4Subframe_GPSX) + 2;
		default:
			break;
	}

	return 0;
}

uint16_t
imet4_subframe_serial(int seq, time_t time)
{
	/* Roughly estimate the start-up time */
	time -= seq / 2;

	return crc16_aug_ccitt((uint8_t*)&time, 4);
}
