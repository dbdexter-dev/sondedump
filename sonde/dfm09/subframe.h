#ifndef dfm09_subframe_h
#define dfm09_subframe_h

#include <time.h>
#include "protocol.h"

/* PTU subframe field accessors */
float dfm09_subframe_temp(const DFM09Subframe_PTU *ptu, const DFM09Calib *calib);
float dfm09_subframe_humidity(const DFM09Subframe_PTU *ptu, const DFM09Calib *calib);
float dfm09_subframe_pressure(const DFM09Subframe_PTU *ptu, const DFM09Calib *calib);

/* GPS subframe field accessors */
uint32_t dfm09_subframe_seq(const DFM09Subframe_GPS *gps);
int dfm09_subframe_time(const DFM09Subframe_GPS *gps);
void dfm09_subframe_date(struct tm *dst, const DFM09Subframe_GPS *gps);
float dfm09_subframe_lat(const DFM09Subframe_GPS *gps);
float dfm09_subframe_lon(const DFM09Subframe_GPS *gps);
float dfm09_subframe_alt(const DFM09Subframe_GPS *gps);
float dfm09_subframe_spd(const DFM09Subframe_GPS *gps);
float dfm09_subframe_hdg(const DFM09Subframe_GPS *gps);
float dfm09_subframe_climb(const DFM09Subframe_GPS *gps);

#endif
