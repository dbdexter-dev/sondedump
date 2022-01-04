#ifndef rs41_frame_h
#define rs41_frame_h

#include "decode/ecc/rs.h"
#include "protocol.h"

/**
 * Descramble the bits inside of the frame, both in terms of ordering and by
 * xor'ing with a PRN sequence
 *
 * @param frame the frame to descramble
 */
void rs41_frame_descramble(RS41Frame *frame);

/**
 * Attempt to correct errors in the frame by using the Reed-Solomon symbols
 *
 * @param frame the frame to correct
 * @param rs the Reed-Solomon decoder to use
 * @return -1 if too many errors
 *         else number of errors corrected
 */
int rs41_frame_correct(RS41Frame *frame, RSDecoder *rs);

int rs41_frame_is_extended(RS41Frame *f);

#endif
