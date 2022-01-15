#ifndef framer_h
#define framer_h

#include <stdint.h>
#include "correlator/correlator.h"
#include "demod/gfsk.h"

typedef struct {
	GFSKDemod gfsk;
	Correlator corr;

	int state;
	size_t offset, sync_offset;
	int inverted;
} Framer;

int framer_init_gfsk(Framer *f, int samplerate, int baudrate, uint64_t syncword, int synclen);
void framer_deinit(Framer *f);

/**
 * Decode a frame, aligning the synchranization marker to the beginning of the
 * given buffer
 *
 * @param framer framer to use to decode a frame
 * @param dst destination buffer to write the frame to
 * @param src source buffer to read samples from
 * @param framelen size of the frame, in bits
 * @param bit_offset pointer to number of bits to assume as correct within dst
 *
 * @return -1 on read failure, -2 on incomplete frame, else offset of the sync marker found
 */
ParserStatus read_frame_gfsk(Framer *f, uint8_t *dst, size_t *bit_offset, size_t framelen, const float *src, size_t len);

#endif
