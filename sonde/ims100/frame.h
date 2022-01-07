#ifndef ims100_frame_h
#define ims100_frame_h

#include "decode/ecc/rs.h"
#include "protocol.h"

void ims100_frame_descramble(IMS100Frame *frame);
int ims100_frame_error_correct(IMS100Frame *frame, RSDecoder *rs);

void ims100_frame_unpack_even(IMS100FrameEven *dst, IMS100Frame *src);
void ims100_frame_unpack_odd(IMS100FrameOdd *dst, IMS100Frame *src);

uint16_t IMS100Frame_seq(const IMS100Frame *frame);

time_t IMS100FrameEven_time(const IMS100FrameEven *frame);
int IMS100FrameEven_seq(const IMS100FrameEven *frame);
float IMS100FrameEven_lat(const IMS100FrameEven *frame);
float IMS100FrameEven_lon(const IMS100FrameEven *frame);
float IMS100FrameEven_alt(const IMS100FrameEven *frame);
float IMS100FrameEven_speed(const IMS100FrameEven *frame);
float IMS100FrameEven_heading(const IMS100FrameEven *frame);
float IMS100FrameEven_temp(const IMS100FrameEven *frame, const IMS100Calibration *calib);
float IMS100FrameEven_rh(const IMS100FrameEven *frame, const IMS100Calibration *calib);

int IMS100FrameOdd_seq(const IMS100FrameOdd *frame);
#endif
