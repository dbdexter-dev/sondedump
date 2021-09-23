#ifndef rs41_subframe_h
#define rs41_subframe_h

#include "protocol.h"

void rs41_parse_subframe(RS41Subframe *sub);

float rs41_subframe_temp(RS41Subframe_PTU *ptu);
float rs41_subframe_humidity(RS41Subframe_PTU *ptu);
float rs41_subframe_temp_humidity(RS41Subframe_PTU *ptu);
float rs41_subframe_pressure(RS41Subframe_PTU *ptu);
float rs41_subframe_pressure_temp(RS41Subframe_PTU *ptu);

#endif
