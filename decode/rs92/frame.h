#ifndef rs92_frame_h
#define rs92_frame_h

#include "decode/ecc/rs.h"
#include "protocol.h"

/**
 * Descramble the bits inside of the frame, both in terms of ordering and by
 * removing the start and stop bits
 *
 * @param frame the frame to descramble
 */
void rs92_frame_descramble(RS92Frame *frame);

/**
 * Attempt to correct errors in the frame by using the Reed-Solomon symbols
 *
 * @param frame the frame to correct
 * @param rs the Reed-Solomon decoder to use
 * @return -1 if too many errors
 *         else number of errors corrected
 */
int rs92_frame_correct(RS92Frame *frame, RSDecoder *rs);

#endif
