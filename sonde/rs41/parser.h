#ifndef rs41_parser_h
#define rs41_parser_h

#include "protocol.h"

/* PTU subframe. Credits to @einergehtnochrein for the calibration data interpretation */
float rs41_temp(RS41Subframe_PTU *ptu, RS41Calibration *calib);
float rs41_humidity(RS41Subframe_PTU *ptu, RS41Calibration *calib);
float rs41_temp_humidity(RS41Subframe_PTU *ptu, RS41Calibration *calib);
float rs41_pressure(RS41Subframe_PTU *ptu, RS41Calibration *calib);

/* GPS position subframe */
float rs41_x(RS41Subframe_GPSPos *gps);
float rs41_y(RS41Subframe_GPSPos *gps);
float rs41_z(RS41Subframe_GPSPos *gps);
float rs41_dx(RS41Subframe_GPSPos *gps);
float rs41_dy(RS41Subframe_GPSPos *gps);
float rs41_dz(RS41Subframe_GPSPos *gps);

#endif
