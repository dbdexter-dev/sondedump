#include <stdint.h>
#include <string.h>
#include "frame.h"
#include "protocol.h"
#include "utils.h"

/* Obtained by autocorrelating the extra data in an ozonosonde */
static const uint8_t prn[RS41_PRN_PERIOD] = {
	0x96, 0x83, 0x3e, 0x51, 0xb1, 0x49, 0x08, 0x98,
	0x32, 0x05, 0x59, 0x0e, 0xf9, 0x44, 0xc6, 0x26,
	0x21, 0x60, 0xc2, 0xea, 0x79, 0x5d, 0x6d, 0xa1,
	0x54, 0x69, 0x47, 0x0c, 0xdc, 0xe8, 0x5c, 0xf1,
	0xf7, 0x76, 0x82, 0x7f, 0x07, 0x99, 0xa2, 0x2c,
	0x93, 0x7c, 0x30, 0x63, 0xf5, 0x10, 0x2e, 0x61,
	0xd0, 0xbc, 0xb4, 0xb6, 0x06, 0xaa, 0xf4, 0x23,
	0x78, 0x6e, 0x3b, 0xae, 0xbf, 0x7b, 0x4c, 0xc1
};

void
rs41_frame_descramble(RS41Frame *frame)
{
	int i;
	uint8_t *raw_frame;

	raw_frame = (uint8_t*)frame;

	/* Reorder bits in the frame and descramble */
	for (i=0; i<RS41_MAX_FRAME_LEN; i++) {
		uint8_t tmp = 0;
		for (int j=0; j<8; j++) {
			tmp |= ((raw_frame[i] >> (7-j)) & 0x1) << j;
		}
		raw_frame[i] = tmp ^ prn[i % LEN(prn)];
	}
}

int
rs41_frame_correct(RS41Frame *frame, RSDecoder *rs)
{
	int i, block, chunk_len;
	int errors, new_errors;
	uint8_t rs_block[RS41_REEDSOLOMON_N];

	if (!rs41_frame_is_extended(frame)) {
		chunk_len = (RS41_DATA_LEN + 1) / RS41_REEDSOLOMON_INTERLEAVING;
		memset(rs_block, 0, LEN(rs_block));
	} else {
		chunk_len = RS41_REEDSOLOMON_K;
	}

	errors = 0;
	for (block=0; block<RS41_REEDSOLOMON_INTERLEAVING; block++) {
		/* Deinterleave */
		for (i=0; i<chunk_len; i++) {
			rs_block[i] = frame->data[RS41_REEDSOLOMON_INTERLEAVING*i + block - 1];
		}
		for (i=0; i<RS41_REEDSOLOMON_T; i++) {
			rs_block[RS41_REEDSOLOMON_K+i] = frame->rs_checksum[i + RS41_REEDSOLOMON_T*block];
		}

		/* Error correct */
		new_errors = rs_fix_block(rs, rs_block);
		if (new_errors < 0 || errors < 0) errors = -1;
		else errors += new_errors;

		/* Reinterleave */
		for (i=0; i<chunk_len; i++) {
			frame->data[RS41_REEDSOLOMON_INTERLEAVING*i + block - 1] = rs_block[i];
		}
		for (i=0; i<RS41_REEDSOLOMON_T; i++) {
			frame->rs_checksum[i + RS41_REEDSOLOMON_T*block] = rs_block[RS41_REEDSOLOMON_K+i];
		}
	}

	return errors;
}

int
rs41_frame_is_extended(RS41Frame *f)
{
	return f->extended_flag == RS41_FLAG_EXTENDED;
}
