#ifndef xdata_h
#define xdata_h

#define DEFAULT_O3_FLOWRATE 30     /* Default O3 flowrate */

/**
 * Derive O3 concentration from raw ozone cell measurements
 *
 * @param pressure          outside air pressure
 * @param o3_current        o3 cell current, in uA
 * @param pump_temp         pump temperature in 'C
 * @param batt_voltage      battery voltage, in V
 * @param o3_flowrate       time taken to force 100mL of air through the sensor, in seconds
 * @return                  ozone concentration, in ppb
 */

float xdata_ozone_ppb(float pressure, float o3_current, float o3_flowrate, float pump_temp);

#endif
