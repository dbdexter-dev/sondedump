#ifndef framer_h
#define framer_h

#include <stdint.h>
#include "correlator/correlator.h"
#include "demod/afsk.h"
#include "demod/gfsk.h"

typedef enum {
	GFSK,
	AFSK
} DemodType;

typedef struct {
	union {
		AFSKDemod afsk;
		GFSKDemod gfsk;
	} demod;
	DemodType type;
	Correlator corr;

	int state;
	size_t offset, sync_offset;
	int inverted;
} Framer;

/**
 * Initialize a GFSK framer object
 *
 * @param f object to init
 * @param samplerate input samplerate
 * @param baudrate baud rate of the signal to decode
 * @param syncword synchronization sequence
 * @param synclen size of the synchronization sequence, in bytes
 *
 * @return 0 on success, nonzero otherwise
 */
int framer_init_gfsk(Framer *f, int samplerate, int baudrate, uint64_t syncword, int synclen);

/**
 * Initialize an AFSK framer object
 *
 * @param f object to init
 * @param samplerate input samplerate
 * @param baudrate baud rate of the signal to decode
 * @param f_mark AFSK frequency for a mark (1) bit
 * @param f_space AFSK frequency for a space (0) bit
 * @param syncword synchronization sequence
 * @param synclen size of the synchronization sequence, in bytes
 */
int framer_init_afsk(Framer *f, int samplerate, int baudrate, float f_mark, float f_space, uint64_t syncword, int synclen);


/**
 * Deinitialize a GFSK framer object
 *
 * @param f object to deinit
 */
void framer_deinit(Framer *f);

/**
 * Decode a frame, aligning the synchranization marker to the beginning of the
 * given buffer
 *
 * @param framer framer to use to decode a frame
 * @param dst destination buffer to write the frame to
 * @param bit_offset pointer to number of bits to assume as correct within dst,
 *        automatically updated by the function
 * @param framelen size of the frame, in bits
 * @param src source buffer to read samples from
 * @param len number of samples in the buffer
 *
 * @return PROCEED if the src buffer has been fully processed
 *         PARSED  if a frame has been decoded into *dst
 */
ParserStatus framer_read(Framer *framer, uint8_t *dst, size_t *bit_offset, size_t framelen, const float *src, size_t len);

#endif
