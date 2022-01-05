#ifndef ims100_frame_h
#define ims100_frame_h

#include "decode/ecc/rs.h"
#include "protocol.h"

void ims100_frame_descramble(IMS100Frame *frame);
int ims100_frame_error_correct(IMS100Frame *frame, RSDecoder *rs);

#endif
