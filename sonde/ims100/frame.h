#ifndef ims100_frame_h
#define ims100_frame_h

#include "decode/ecc/rs.h"
#include "protocol.h"

typedef struct {
	uint16_t ref, temp, rh, rh_temp;
} IMS100FrameADC;


void ims100_frame_descramble(IMS100ECCFrame *frame);
int ims100_frame_error_correct(IMS100ECCFrame *frame, RSDecoder *rs);

void ims100_frame_unpack(IMS100Frame *dst, const IMS100ECCFrame *src);

uint16_t ims100_frame_seq(const IMS100Frame *frame);
float    ims100_frame_temp(const IMS100FrameADC *adc, const IMS100Calibration *calib);
float    ims100_frame_rh(const IMS100FrameADC *adc, const IMS100Calibration *calib);
uint16_t ims100_frame_subtype(const IMS100Frame *frame);

#endif
