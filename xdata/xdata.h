#ifndef xdata_h
#define xdata_h

#define DEFAULT_O3_FLOWRATE 30     /* Default O3 flowrate */

/**
 * Derive O3 partial pressure from raw ozone cell measurements
 *
 * @param o3_current        o3 cell current, in uA
 * @param pump_temp         pump temperature in 'C
 * @param batt_voltage      battery voltage, in V
 * @param o3_flowrate       time taken to force 100mL of air through the sensor, in seconds
 * @return                  ozone partial pressure, [mPa]
 */

float xdata_ozone_mpa(float o3_current, float o3_flowrate, float pump_temp);

float xdata_ozone_mpa_to_ppb(float o3_mpa, float pressure);

#endif
