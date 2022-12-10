#include <math.h>
#include "physics.h"
#include "utils.h"

float
altitude_to_pressure(float alt)
{
	const float g0 = 9.80665;
	const float M = 0.0289644;
	const float R_star = 8.3144598;

	const float hbs[] = {0.0,      11000.0, 20000.0, 32000.0, 47000.0, 51000.0, 77000.0};
	const float Lbs[] = {-0.0065,  0.0,     0.001,   0.0028,  0.0,     -0.0028, -0.002};
	const float Pbs[] = {101325.0, 22632.1, 5474.89, 868.02,  110.91,  66.94,   3.96};
	const float Tbs[] = {288.15,   216.65,  216.65,  228.65,  270.65,  270.65,  214.65};

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
	const float tmp = (logf(rh / 100.0) + (17.27 * temp / (237.3 + temp))) / 17.27;
	return 237.3 * tmp  / (1 - tmp);
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
	const float coeffs[] = {-0.493158, 1.0 + 4.6094296e-3, -1.3746454e-5, 1.2743214e-8};
	float T, p;
	int i;

	temp += 273.15;

	T = 0;
	for (i=LEN(coeffs)-1; i>=0; i--) {
		T *= temp;
		T += coeffs[i];
	}

	p = expf(-5800.2206 / T
		  + 1.3914993
		  + 6.5459673 * logf(T)
		  - 4.8640239e-2 * T
		  + 4.1764768e-5 * T * T
		  - 1.4452093e-8 * T * T * T);

	return p / 100.0;

}

