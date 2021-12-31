#ifndef dfm09_subframe_h
#define dfm09_subframe_h

#include <time.h>
#include "protocol.h"

/* PTU subframe */
float dfm09_subframe_temp(DFM09Subframe_PTU *ptu, DFM09Calib *calib);
float dfm09_subframe_humidity(DFM09Subframe_PTU *ptu, DFM09Calib *calib);
float dfm09_subframe_pressure(DFM09Subframe_PTU *ptu, DFM09Calib *calib);

/* GPS subframe */
int dfm09_subframe_time(DFM09Subframe_GPS *gps);
void dfm09_subframe_date(struct tm *dst, DFM09Subframe_GPS *gps);
float dfm09_subframe_lat(DFM09Subframe_GPS *gps);
float dfm09_subframe_lon(DFM09Subframe_GPS *gps);
float dfm09_subframe_alt(DFM09Subframe_GPS *gps);
float dfm09_subframe_spd(DFM09Subframe_GPS *gps);
float dfm09_subframe_hdg(DFM09Subframe_GPS *gps);
float dfm09_subframe_climb(DFM09Subframe_GPS *gps);

#endif
