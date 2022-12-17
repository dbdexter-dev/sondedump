#ifndef imet4_frame_h
#define imet4_frame_h

#include "protocol.h"

/**
 * Process frame so as to recover the raw encoded data
 *
 * @param src   frame to descramble
 * @param dst   frame to write descrambled data into
 */
void imet4_frame_descramble(IMET4Frame *dst, IMET4Frame *src);

#endif
