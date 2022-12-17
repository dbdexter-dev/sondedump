#ifndef rs41_frame_h
#define rs41_frame_h

#include "decode/ecc/rs.h"
#include "protocol.h"

/**
 * Descramble the bits inside of the frame, both in terms of ordering and by
 * xor'ing with a PRN sequence
 *
 * @param src   the frame to descramble
 * @param dst   destination to write the resulting frame into
 */
void rs41_frame_descramble(RS41Frame *dst, RS41Frame *src);

/**
 * Attempt to correct errors in the frame by using the Reed-Solomon symbols
 *
 * @param frame the frame to correct
 * @param rs the Reed-Solomon decoder to use
 * @return -1 if too many errors
 *         else number of errors corrected
 */
int rs41_frame_correct(RS41Frame *frame, RSDecoder *rs);


/**
 * Check if the frame contains an XDATA field
 *
 * @param f frame to analyze
 * @return 1 if extended, 0 otherwise
 */
int rs41_frame_is_extended(RS41Frame *f);

#endif
