#ifndef gpstime_h
#define gpstime_h

#include <stdint.h>
#include <time.h>

time_t gps_time_to_utc(uint16_t week, uint32_t ms);

#endif
