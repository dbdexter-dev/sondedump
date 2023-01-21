#ifndef ims100_frame_h
#define ims100_frame_h

#include "decode/ecc/rs.h"
#include "protocol.h"

/**
 * Reorder bits within the frame so that they make sense
 *
 * @param frame frame to descramble
 */
void ims100_frame_descramble(IMS100ECCFrame *frame);

/**
 * Perform error correction on the frame
 *
 * @param frame frame to correct
 * @param rs Reed-Solomon decoder to use
 * @return -1 if uncorrectable errors were detected
 *         number of errors corrected otherwise
 */
int ims100_frame_error_correct(IMS100ECCFrame *frame, const RSDecoder *rs);

/**
 * Strip error-correcting bits from the given frame, reconstructing the payload
 *
 * @param dst destination buffer to write the compacted frame to
 * @param src source buffer containing the raw frame (including RS bits)
 */
void ims100_frame_unpack(IMS100Frame *dst, const IMS100ECCFrame *src);


#endif
