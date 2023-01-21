#ifndef dfm09_parser_h
#define dfm09_parser_h

#include <time.h>
#include "protocol.h"

/* PTU subframe field accessors */
float dfm09_temp(const DFM09Subframe_PTU *ptu, const DFM09Calib *calib);
float dfm09_humidity(const DFM09Subframe_PTU *ptu, const DFM09Calib *calib);
float dfm09_pressure(const DFM09Subframe_PTU *ptu, const DFM09Calib *calib);

/* GPS subframe field accessors */
uint32_t dfm09_seq(const DFM09Subframe_GPS *gps);
int dfm09_time(const DFM09Subframe_GPS *gps);
void dfm09_date(struct tm *dst, const DFM09Subframe_GPS *gps);
float dfm09_lat(const DFM09Subframe_GPS *gps);
float dfm09_lon(const DFM09Subframe_GPS *gps);
float dfm09_alt(const DFM09Subframe_GPS *gps);
float dfm09_speed(const DFM09Subframe_GPS *gps);
float dfm09_heading(const DFM09Subframe_GPS *gps);
float dfm09_climb(const DFM09Subframe_GPS *gps);

#endif
