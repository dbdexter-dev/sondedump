#ifndef dfm09_frame_h
#define dfm09_frame_h

#include "protocol.h"

/**
 * Deinterleave bits within the frame
 *
 * @param frame frame to deinterleave
 */
void dfm09_frame_deinterleave(DFM09ECCFrame *frame);

/**
 * Perform error correction on the given frame
 *
 * @param frame frame to error correct
 * @return -1 if too many errors
 *         else number of errors corrected
 */
int  dfm09_frame_correct(DFM09ECCFrame *frame);

/**
 * Strip error-correcting bits from the given frame, reconstructing the payload
 *
 * @param dst destination buffer to write the compacted frame to
 * @param src source buffer containing the raw frame
 */
void dfm09_frame_unpack(DFM09Frame *dst, const DFM09ECCFrame *src);
#endif
