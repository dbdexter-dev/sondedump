#include "utils.h"
#include "xdata.h"

/* https://www.en-sci.com/wp-content/uploads/2020/02/Ozonesonde-Flight-Preparation-Manual.pdf */
/* https://www.vaisala.com/sites/default/files/documents/Ozone%20Sounding%20with%20Vaisala%20Radiosonde%20RS41%20User%27s%20Guide%20M211486EN-C.pdf */

static float o3_correction_factor(float pressure);

/* Pressure-dependent O3 correction factors */
static const float _cfPressure[] = {3, 5, 7, 10, 15, 20, 30, 50, 70, 100, 150, 200};
static const float _cfFactor[]   = {1.24, 1.124, 1.087, 1.066, 1.048, 1.041,
                                   1.029, 1.018, 1.013, 1.007, 1.002, 1.000};


float
xdata_ozone_mpa(float o3_current, float o3_flowrate, float pump_temp)
{
	float o3_mpa;

	o3_mpa = 4.307e-3 * o3_current * pump_temp * o3_flowrate;

	return o3_mpa;
}

float
xdata_ozone_mpa_to_ppb(float o3_mpa, float pressure)
{
	float o3_ppb;

	o3_ppb = o3_mpa * o3_correction_factor(pressure) * 1000.0 / pressure;

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
