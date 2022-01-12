#ifndef m10_frame_h
#define m10_frame_h

#include "protocol.h"

/**
 * Reorder bits within the frame so that they make sense
 *
 * @param frame frame to descramble
 */
void m10_frame_descramble(M10Frame *frame);

/**
 * Perform error correction on the given frame
 *
 * @param frame frame to error correct
 * @return -1 if too many errors
 *         else number of errors corrected
 */
int m10_frame_correct(M10Frame *frame);

#endif
