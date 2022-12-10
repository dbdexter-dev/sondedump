#include <stdio.h>
#include "physics.h"
#include "utils.h"
#include "xdata.h"

/* https://www.en-sci.com/wp-content/uploads/2020/02/Ozonesonde-Flight-Preparation-Manual.pdf */
/* https://www.vaisala.com/sites/default/files/documents/Ozone%20Sounding%20with%20Vaisala%20Radiosonde%20RS41%20User%27s%20Guide%20M211486EN-C.pdf */

#define O3_FLOWRATE 30     /* Time taken by the pump to force 100mL of air through the sensor, in seconds */

static float o3_correction_factor(float pressure);
static float o3_concentration(float pressure, float pump_temp, float o3_current);

/* Pressure-dependent O3 correction factors */
static const float _cfPressure[] = {3, 5, 7, 10, 15, 20, 30, 50, 70, 100, 150, 200};
static const float _cfFactor[]   = {1.24, 1.124, 1.087, 1.066, 1.048, 1.041,
                                   1.029, 1.018, 1.013, 1.007, 1.002, 1.000};


void
xdata_decode(SondeXdata *dst, float curPressure, const char *asciiData, int len)
{
	unsigned int r_pumpTemp, r_o3Current, r_battVoltage, r_pumpCurrent, r_extVoltage;
	float pumpTemp, o3Current;
	unsigned int instrumentID, instrumentNum;

	while (len > 0) {
		sscanf(asciiData, "%02X%02X", &instrumentID, &instrumentNum);
		asciiData += 4;
		len -= 4;

		switch (instrumentID) {
		case XDATA_ENSCI_OZONE:
			if (sscanf(asciiData, "%04X%05X%02X%03X%02X",
			           &r_pumpTemp, &r_o3Current, &r_battVoltage, &r_pumpCurrent, &r_extVoltage) == 5) {
				asciiData += 16;
				pumpTemp = (r_pumpTemp & 0x8000 ? -1 : 1) * 0.001 * (r_pumpTemp & 0x7FFF) + 273.15;
				o3Current = r_o3Current * 1e-5;

				dst->o3_ppb = o3_concentration(curPressure, pumpTemp, o3Current);
			} else {
				/* Diagnostic data */
				asciiData += 17;
			}
			break;
		default:
			break;
		}
	}
}

static float
o3_concentration(float pressure, float pump_temp, float o3_current)
{
	float o3_pressure, o3_ppb;

	o3_pressure = 4.307e-3 * o3_current * pump_temp * O3_FLOWRATE * o3_correction_factor(pressure);
	o3_ppb = o3_pressure * 1000.0 / pressure;

	return o3_ppb;
}

static float
o3_correction_factor(float pressure)
{
	for (int i=0; i<(int)LEN(_cfFactor); i++) {
		if (pressure < _cfPressure[i]) return _cfFactor[i];
	}
	return 1.0;
}
