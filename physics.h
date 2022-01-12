#ifndef physics_h
#define physics_h

/**
 * Calculate pressure at a given altitude
 *
 * @param alt altitude
 * @return pressure at that altitude (hPa)
 */
float altitude_to_pressure(float alt);

/**
 * Calculate dew point
 *
 * @param temp temperature (degrees Celsius)
 * @param rh relative humidity (0-100%)
 * @return dew point (degrees Celsius)
 */
float dewpt(float temp, float rh);

/**
 * Calculate saturation mixing ratio given temperature and pressure
 *
 * @param temp temperature (degrees Celsius)
 * @param p pressure (hPa)
 * @return saturation mixing ratio (g/kg)
 */
float sat_mixing_ratio(float temp, float p);

/**
 * Calculate the water vapour saturation pressure at a temeprature (Hyland/Wexler)
 *
 * @param temp temperature ('C)
 * @return saturation pressure (Pa)
 */
float wv_sat_pressure(float temp);


#endif
