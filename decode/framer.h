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
	int sync_offset;
	size_t offset;
	size_t framelen;
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
 * @param framelen frame length, in bits
 *
 * @return 0 on success, nonzero otherwise
 */
int framer_init_gfsk(Framer *f, int samplerate, int baudrate, size_t framelen, uint64_t syncword, int synclen);

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
 * @param framelen frame length, in bits
 */
int framer_init_afsk(Framer *f, int samplerate, int baudrate, size_t framelen, float f_mark, float f_space, uint64_t syncword, int synclen);


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
 * @param src source buffer to read samples from
 * @param len number of samples in the buffer
 *
 * @return PROCEED if the src buffer has been fully processed
 *         PARSED  if a frame has been decoded into *dst
 */
ParserStatus framer_read(Framer *framer, void *dst, const float *src, size_t len);

void framer_adjust(Framer *framer, void *v_dst, size_t bits_delta);

#endif
