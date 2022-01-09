#ifndef ims100_frame_h
#define ims100_frame_h

#include "decode/ecc/rs.h"
#include "protocol.h"

void ims100_frame_descramble(IMS100ECCFrame *frame);
int ims100_frame_error_correct(IMS100ECCFrame *frame, RSDecoder *rs);

void ims100_frame_unpack(IMS100Frame *dst, const IMS100ECCFrame *src);

uint16_t IMS100Frame_seq(const IMS100Frame *frame);
float IMS100Frame_temp(const IMS100Frame *frame, const IMS100Calibration *calib);
float IMS100Frame_rh(const IMS100Frame *frame, const IMS100Calibration *calib);
uint16_t IMS100Frame_subtype(const IMS100Frame *frame);

#endif
