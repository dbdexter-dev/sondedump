#ifndef dfm09_subframe_h
#define dfm09_subframe_h

#include "protocol.h"

/* PTU subframe */
float dfm09_subframe_temp(DFM09Subframe_PTU *ptu, DFM09Calib *calib);
float dfm09_subframe_humidity(DFM09Subframe_PTU *ptu, DFM09Calib *calib);
float dfm09_subframe_pressure(DFM09Subframe_PTU *ptu, DFM09Calib *calib);

#endif
