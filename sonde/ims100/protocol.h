#ifndef ims100_protocol_h
#define ims100_protocol_h

#include <stdint.h>
#include <math.h>
#include <time.h>
#include "utils.h"

#define IMS100_BAUDRATE 2400

#define IMS100_SYNCWORD 0xaaa56a659a99559a
#define IMS100_SYNC_LEN 8
#define IMS100_FRAME_LEN 1200

#define IMS100_SUBFRAME_LEN 300
#define IMS100_SUBFRAME_BCHLEN 12
#define IMS100_SUBFRAME_VALUELEN 17
#define IMS100_MESSAGE_LEN (2 * IMS100_SUBFRAME_VALUELEN + IMS100_SUBFRAME_BCHLEN)

#define IMS100_REEDSOLOMON_N 63
#define IMS100_REEDSOLOMON_K 51
#define IMS100_REEDSOLOMON_T 4
#define IMS100_REEDSOLOMON_POLY 0x61

#define IMS100_DATA_VALID(bits, mask) (((bits) & (mask)) == (mask))

#define IMS100_EVEN_MASK_SPEED   0x000002
#define IMS100_EVEN_MASK_HEADING 0x000004
#define IMS100_EVEN_MASK_ALT     0x000060
#define IMS100_EVEN_MASK_LON     0x000180
#define IMS100_EVEN_MASK_LAT     0x000600
#define IMS100_EVEN_MASK_DATE    0x000800
#define IMS100_EVEN_MASK_TIME    0x003000
#define IMS100_EVEN_MASK_SEQ     0x800000

extern uint8_t ims100_bch_roots[];

/* Even & odd frame types {{{ */
typedef struct {
	/* Offset 0 */
	uint8_t seq[2];
	uint8_t _pad0[18];
	uint8_t ms[2];
	uint8_t hour;
	uint8_t min;

	/* Offset 24 */
	uint8_t date[2];
	uint8_t lat[4];
	uint8_t lon[4];
	uint8_t alt[3];

	uint8_t _pad1[5];
	uint8_t heading[2];
	uint8_t speed[2];

	uint8_t padding[2];

	uint32_t valid;
} __attribute__((packed)) IMS100FrameEven;

typedef struct {
	uint8_t seq[2];
	uint8_t data[46];

	uint32_t valid;
} __attribute__((packed)) IMS100FrameOdd;
/* }}} */

typedef struct {
	uint8_t syncword[3];
	uint8_t seq[2];

	uint8_t data[70];
} __attribute__((packed)) IMS100Frame;

#endif
