#include <stdio.h>
#include "subframe.h"
#include "utils.h"

float
rs92_subframe_xyz(float *x, float *y, float *z, RS92Subframe_GPSRaw *raw)
{
	uint8_t prns[RS92_GPS_COUNT];
	uint16_t raw_prn;
	int i, j;

	/* Extract PRN identifiers */
	for (i=0; i<4; i++) {
		raw_prn = raw->prn[i];
		for (j=0; j<3; j++) {
			prns[i*3 + 2-j] = raw_prn & 0x1F;
			raw_prn >>= 5;
		}
	}

	*x = 0;
	*y = 0;
	*z = 0;

	return 0;
}
