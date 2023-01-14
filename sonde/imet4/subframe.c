#include <stddef.h>
#include "decode/ecc/crc.h"
#include "subframe.h"
#include "xdata/xdata.h"
#include "log/log.h"

size_t
imet4_subframe_len(IMET4Subframe *sf)
{
	/* If start of header is missing, discard frame */
	if (sf->soh != 0x01) return 0;

	switch (sf->type) {
	case IMET4_SFTYPE_PTU:
		return sizeof(IMET4Subframe_PTU) + 2;
	case IMET4_SFTYPE_GPS:
		return sizeof(IMET4Subframe_GPS) + 2;
	case IMET4_SFTYPE_XDATA:
		return offsetof(IMET4Subframe, data) + 1 + sf->data[0] + 2;
	case IMET4_SFTYPE_PTUX:
		return sizeof(IMET4Subframe_PTUX) + 2;
	case IMET4_SFTYPE_GPSX:
		return sizeof(IMET4Subframe_GPSX) + 2;
	default:
		break;
	}

	/* Unknown frame type */
	return 0;
}

float
imet4_subframe_xdata_ozone(float pressure, IMET4Subframe_XDATA_Ozone *sf)
{
	const float cell_current = (sf->cell_current[0] << 8 | sf->cell_current[1]) / 1000.0f;
	const float pump_temp = (sf->pump_temp[0] << 8 | sf->pump_temp[1]) / 100.0f;

	return xdata_ozone_ppb(pressure, cell_current, DEFAULT_O3_FLOWRATE, pump_temp);
}
