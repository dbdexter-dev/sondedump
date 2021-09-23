#ifndef rs41_frame_h
#define rs41_frame_h

#include "protocol.h"

void rs41_frame_descramble(RS41Frame *frame);
int rs41_frame_correct(RS41Frame *frame);

#endif
