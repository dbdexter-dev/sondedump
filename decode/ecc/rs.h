#ifndef rs_h
#define rs_h

#include <stdint.h>
#include <stdlib.h>

typedef struct {
	int n, k, t, first_root;
	uint8_t *alpha, *logtable, *zeroes, *gaproots;
} RSDecoder;

/**
 * Initialize the given Reed-Solomon decoder
 *
 * @param d decoder to initialize
 * @param n message length, in symbols
 * @param k payload length, in symbols
 * @param gen_poly generator polynomial
 * @param first_root first root of the generator to use
 * @param root_skip distance between consecutive roots
 */
int rs_init(RSDecoder *d, int n, int k, unsigned gen_poly, uint8_t first_root, int root_skip);

/**
 * Initialize the given Reed-Solomon decoder, in BCH mode
 *
 * @param d decoder to initialize
 * @param n message length, in symbols
 * @param k payload length, in symbols
 * @param gen_poly generator polynomial
 * @param roots roots of the generator to use
 * @param root_count number of roots in *roots
 */
int bch_init(RSDecoder *d, int n, int k, unsigned gen_poly, const uint8_t *roots, int root_count);

/**
 * Deinitialize the Reed-Solomon decoder
 *
 * @param d decoder to deinitialize
 */
void rs_deinit(RSDecoder *d);

/**
 * Attempt to fix the data inside a block
 *
 * @param d the RS decoder to use
 * @param c the block to error correct. If all errors can be corrected, bytes
 *          will be modified in-place.
 * @return -1  errors could not be corrected
 *         >=0 number of errors corrected. The VCDU argument can be assumed to
 *             be free of any errors in this case.
 */
int rs_fix_block(const RSDecoder *d, uint8_t *c);

#endif
