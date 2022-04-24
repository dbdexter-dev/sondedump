#include <stdio.h>
#include "physics.h"
#include "utils.h"
#include "xdata.h"

/* https://www.en-sci.com/wp-content/uploads/2020/02/Ozonesonde-Flight-Preparation-Manual.pdf */
/* https://www.vaisala.com/sites/default/files/documents/Ozone%20Sounding%20with%20Vaisala%20Radiosonde%20RS41%20User%27s%20Guide%20M211486EN-C.pdf */

static char xdata_str[128];

char*
xdata_decode(float curPressure, const char *asciiData, int len)
{
	unsigned int instrumentID, instrumentNum;
	char *dst = xdata_str;

	while (len > 0) {
		sscanf(asciiData, "%02X%02X", &instrumentID, &instrumentNum);
		asciiData += 4;
		len -= 4;

		switch (instrumentID) {
			case XDATA_ENSCI_OZONE:
				{
					unsigned int r_pumpTemp, r_o3Current, r_battVoltage, r_pumpCurrent, r_extVoltage;
					float pumpTemp, o3Current, o3PPB;

					if (sscanf(asciiData, "%04X%05X%02X%03X%02X", &r_pumpTemp, &r_o3Current, &r_battVoltage, &r_pumpCurrent, &r_extVoltage) == 5) {
						asciiData += 16;
						pumpTemp = (r_pumpTemp & 0x8000 ? -1 : 1) * 0.001 * (r_pumpTemp & 0x7FFF) + 273.15;
						o3Current = r_o3Current * 1e-5;

						o3PPB = o3_concentration(curPressure, pumpTemp, o3Current);
						dst += sprintf(dst, "O3=%.2fppb ", o3PPB);

					} else {
						/* Diagnostic data */
						asciiData += 17;
					}
				}

				break;
			default:
				break;
		}
	}

	return xdata_str;
}
