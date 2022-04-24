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

/**
 * Calculate the ozone concentration reported by an XDATA instrument
 * https://www.en-sci.com/wp-content/uploads/2020/02/Ozonesonde-Flight-Preparation-Manual.pdf
 * https://www.vaisala.com/sites/default/files/documents/Ozone%20Sounding%20with%20Vaisala%20Radiosonde%20RS41%20User%27s%20Guide%20M211486EN-C.pdf
 *
 * @param pressure      current air pressure [hPa]
 * @param pump_temp     pump temperature [K]
 * @param o3_current    O3 current [uA]
 * @return              ozone concentration, in ppb
 */
float o3_concentration(float pressure, float pump_temp, float o3_current);


#endif
