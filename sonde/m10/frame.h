#ifndef m10_frame_h
#define m10_frame_h

#include "protocol.h"

void m10_frame_descramble(M10Frame *frame);
int m10_frame_correct(M10Frame *frame);

#endif
