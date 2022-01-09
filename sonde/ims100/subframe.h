#ifndef ims100_subframe_h
#define ims100_subframe_h
#include "protocol.h"

time_t IMS100FrameGPS_time(const IMS100FrameGPS *frame);
float IMS100FrameGPS_lat(const IMS100FrameGPS *frame);
float IMS100FrameGPS_lon(const IMS100FrameGPS *frame);
float IMS100FrameGPS_alt(const IMS100FrameGPS *frame);
float IMS100FrameGPS_speed(const IMS100FrameGPS *frame);
float IMS100FrameGPS_heading(const IMS100FrameGPS *frame);

#endif
