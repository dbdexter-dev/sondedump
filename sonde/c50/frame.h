#ifndef c50_frame_h
#define c50_frame_h

#include "protocol.h"

void c50_frame_descramble(C50Frame *dst, const C50RawFrame *src);
int c50_frame_correct(C50Frame *frame);

#endif
