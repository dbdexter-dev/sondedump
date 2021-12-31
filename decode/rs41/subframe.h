#ifndef rs41_subframe_h
#define rs41_subframe_h

#include "protocol.h"

void rs41_parse_subframe(RS41Subframe *sub);

/* PTU subframe. Credits to @zilog80 for the calibration data interpretation */
float rs41_subframe_temp(RS41Subframe_PTU *ptu, RS41Calibration *calib);
float rs41_subframe_humidity(RS41Subframe_PTU *ptu, RS41Calibration *calib);
float rs41_subframe_temp_humidity(RS41Subframe_PTU *ptu, RS41Calibration *calib);
float rs41_subframe_pressure(RS41Subframe_PTU *ptu, RS41Calibration *calib);
float rs41_subframe_temp_pressure(RS41Subframe_PTU *ptu);

/* GPS position subframe */
float rs41_subframe_x(RS41Subframe_GPSPos *gps);
float rs41_subframe_y(RS41Subframe_GPSPos *gps);
float rs41_subframe_z(RS41Subframe_GPSPos *gps);
float rs41_subframe_dx(RS41Subframe_GPSPos *gps);
float rs41_subframe_dy(RS41Subframe_GPSPos *gps);
float rs41_subframe_dz(RS41Subframe_GPSPos *gps);

#endif
