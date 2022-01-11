#include <math.h>
#include <string.h>
#include "frame.h"
#include "utils.h"

static int parity(uint8_t x);
static int hamming(uint8_t *data, int len);

void
dfm09_frame_deinterleave(DFM09ECCFrame *frame)
{
	DFM09ECCFrame deinterleaved;
	uint8_t *src;
	int i, j, idx;

	deinterleaved.sync[0] = frame->sync[0];
	deinterleaved.sync[1] = frame->sync[1];

	src = frame->ptu;
	for (i=0; i<(int)sizeof(frame->ptu); i++) {
		for (j=0; j<8; j++) {
			idx = (i*8+j) % DFM09_INTERLEAVING_PTU + (i - i%DFM09_INTERLEAVING_PTU);
			deinterleaved.ptu[idx] <<= 1;
			deinterleaved.ptu[idx] |= src[i] >> (7-j) & 0x01;
		}
	}

	src = frame->gps;
	for (i=0; i<(int)sizeof(frame->gps); i++) {
		for (j=0; j<8; j++) {
			idx = (i*8+j) % DFM09_INTERLEAVING_GPS + (i - i%DFM09_INTERLEAVING_GPS);
			deinterleaved.gps[idx] <<= 1;
			deinterleaved.gps[idx] |= src[i] >> (7-j) & 0x01;
		}
	}

	memcpy(frame, &deinterleaved, sizeof(*frame));
}

int
dfm09_frame_correct(DFM09ECCFrame *frame)
{
	int ptuErrcount, gpsErrcount;

	ptuErrcount = hamming(frame->ptu, sizeof(frame->ptu));
	gpsErrcount = hamming(frame->gps, sizeof(frame->gps));

	if (ptuErrcount < 0 || gpsErrcount < 0) return -1;

	return ptuErrcount + gpsErrcount;
}

void
dfm09_frame_unpack(DFM09Frame *dst, const DFM09ECCFrame *src)
{
	int i;

	dst->ptu.type = src->ptu[0] >> 4;
	for (i=0; i<(int)sizeof(dst->ptu.data); i++) {
		dst->ptu.data[i] = (src->ptu[1+2*i] & 0xF0) | (src->ptu[1+2*i+1] >> 4);
	}

	dst->gps[0].type = src->gps[12] >> 4;
	for (i=0; i<(int)sizeof(dst->gps[0].data); i++) {
		dst->gps[0].data[i] = (src->gps[2*i] & 0xF0) | (src->gps[2*i+1] >> 4);
	}

	dst->gps[1].type = src->gps[25] >> 4;
	for (i=0; i<(int)sizeof(dst->gps[1].data); i++) {
		dst->gps[1].data[i] = (src->gps[13+2*i] & 0xF0) | (src->gps[13+2*i+1] >> 4);
	}
}

/* Static functions {{{ */
static int
parity(uint8_t x)
{
	int ret;

	for (ret = 0; x; ret++) {
		x &= x-1;
	}

	return ret % 2;
}

static int
hamming(uint8_t *data, int len)
{
	const uint8_t hamming_bitmasks[] = {0xaa, 0x66, 0x1e, 0xff};
	int errpos, errcount = 0;
	int i, j;

	for (i=0; i<len; i++) {
		errpos = 0;
		for (j=0; j<(int)sizeof(hamming_bitmasks); j++) {
			errpos += (1 << j) * parity(data[i] & hamming_bitmasks[j]);
		}

		if (errpos > 7) return -1;

		if (errpos) {
			errcount++;
			data[i] ^= 1 << (8 - errpos);
		}
	}

	return errcount;
}
/* }}} */
