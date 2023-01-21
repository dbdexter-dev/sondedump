#ifndef imet4_parser_h
#define imet4_parser_h

#include "protocol.h"

float imet4_ptu_temp(const IMET4Subframe_PTU *ptu);
float imet4_ptu_rh(const IMET4Subframe_PTU *ptu);
float imet4_ptu_pressure(const IMET4Subframe_PTU *ptu);

float imet4_gps_lat(const IMET4Subframe_GPS *gps);
float imet4_gps_lon(const IMET4Subframe_GPS *gps);
float imet4_gps_alt(const IMET4Subframe_GPS *gps);
time_t imet4_gps_time(const IMET4Subframe_GPS *gps);

time_t imet4_gpsx_time(const IMET4Subframe_GPSX *gps);
float imet4_gpsx_speed(const IMET4Subframe_GPSX *gps);
float imet4_gpsx_heading(const IMET4Subframe_GPSX *gps);
float imet4_gpsx_climb(const IMET4Subframe_GPSX *gps);

#endif
