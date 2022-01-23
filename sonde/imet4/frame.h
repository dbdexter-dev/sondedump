#ifndef imet4_frame_h
#define imet4_frame_h

#include "protocol.h"

/**
 * Process frame so as to recover the raw encoded data
 *
 * @param frame frame to descramble
 */
void imet4_frame_descramble(IMET4Frame *frame);

#endif
