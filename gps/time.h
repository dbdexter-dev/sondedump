#ifndef gpstime_h
#define gpstime_h

#include <stdint.h>
#include <time.h>

/**
 * Convert a GPS (week, ms) pair into an epoch timestamp
 *
 * @param week GPS week
 * @param ms GPS time of the week (ToW)
 * @return corresponding UTC time
 */
time_t gps_time_to_utc(uint16_t week, uint32_t ms);

#endif
