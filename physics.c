#include <math.h>
#include "physics.h"
#include "utils.h"

#define O3_FLOWRATE 30     /* Time taken by the pump to force 100mL of air through the sensor, in seconds */

static float o3_correction_factor(float pressure);

/* Pressure-dependent O3 correction factors */
static const float _cfPressure[] = {3, 5, 7, 10, 15, 20, 30, 50, 70, 100, 150, 200};
static const float _cfFactor[]   = {1.24, 1.124, 1.087, 1.066, 1.048, 1.041, 1.029, 1.018, 1.013, 1.007, 1.002, 1};

float
altitude_to_pressure(float alt)
{
	const float g0 = 9.80665;
	const float M = 0.0289644;
	const float R_star = 8.3144598;

	const float hbs[] = {0,       11000,   20000,   32000,  47000,  51000,  77000};
	const float Lbs[] = {-0.0065, 0,       0.001,   0.0028, 0.0,   -0.0028, -0.002};
	const float Pbs[] = {101325,  22632.1, 5474.89, 868.02, 110.91, 66.94,  3.96};
	const float Tbs[] = {288.15,  216.65,  216.65,  228.65, 270.65, 270.65, 214.65};

	float Lb, Pb, Tb, hb;
	int b;

	for (b=0; b<(int)LEN(Lbs)-1; b++) {
		if (alt < hbs[b+1]) {
			Lb = Lbs[b];
			Pb = Pbs[b];
			Tb = Tbs[b];
			hb = hbs[b];
			break;
		}
	}

	if (b == (int)LEN(Lbs) - 1) {
		Lb = Lbs[b];
		Pb = Pbs[b];
		Tb = Tbs[b];
		hb = hbs[b];
	}

	if (Lb != 0) {
		return 1e-2 * Pb * powf((Tb + Lb * (alt - hb)) / Tb, - (g0 * M) / (R_star * Lb));
	}
	return 1e-2 * Pb * expf(-g0 * M * (alt - hb) / (R_star * Tb));
}

float
dewpt(float temp, float rh)
{
	const float tmp = (logf(rh / 100.0f) + (17.27f * temp / (237.3f + temp))) / 17.27f;
	return 237.3f * tmp  / (1 - tmp);
}

float
sat_mixing_ratio(float temp, float p)
{
	const float wv_pressure = 610.97e-3 * expf((17.625*temp)/(temp+243.04));
	const float wice_pressure = 611.21e-3 * expf((22.587*temp)/(temp+273.86));

	if (temp < 0) {
		return 621.97 * wice_pressure / (p - wice_pressure);
	} else {
		return 621.97 * wv_pressure / (p - wv_pressure);
	}
}

float
wv_sat_pressure(float temp)
{
	const float coeffs[] = {-0.493158, 1 + 4.6094296e-3, -1.3746454e-5, 1.2743214e-8};
	float T, p;
	int i;

	temp += 273.15f;

	T = 0;
	for (i=LEN(coeffs)-1; i>=0; i--) {
		T *= temp;
		T += coeffs[i];
	}

	p = expf(-5800.2206f / T
		  + 1.3914993f
		  + 6.5459673f * logf(T)
		  - 4.8640239e-2f * T
		  + 4.1764768e-5f * T * T
		  - 1.4452093e-8f * T * T * T);

	return p / 100.0f;

}

float
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
	return 1;
}
