#ifndef ims100_subframe_h
#define ims100_subframe_h
#include "protocol.h"

/* Accessors for GPS subframe fields */
time_t ims100_subframe_time(const IMS100FrameGPS *frame);
float ims100_subframe_lat(const IMS100FrameGPS *frame);
float ims100_subframe_lon(const IMS100FrameGPS *frame);
float ims100_subframe_alt(const IMS100FrameGPS *frame);
float ims100_subframe_speed(const IMS100FrameGPS *frame);
float ims100_subframe_heading(const IMS100FrameGPS *frame);

/* TODO accessors for the other subframe type */

#endif
