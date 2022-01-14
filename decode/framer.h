#ifndef framer_h
#define framer_h

#include <stdint.h>
#include "correlator/correlator.h"
#include "demod/gfsk.h"

/**
 * Decode a frame, aligning the synchranization marker to the beginning of the
 * given buffer
 *
 * @param demod demodulator to use to read bits
 * @param corr correlator to use to align the frame
 * @param dst destination buffer to write the frame to
 * @param read function to use to read raw samples from
 * @param framelen size of the frame
 * @param bit_offset number of bits to assume as correct within dst
 *
 * @return -1 on read failure, else offset of the sync marker found
 */
int read_frame_gfsk(GFSKDemod *gfsk, Correlator *corr, uint8_t *dst, int (*read)(float *dst, size_t len), size_t framelen, size_t bit_offset);

#endif
