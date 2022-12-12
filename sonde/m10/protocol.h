#ifndef m10_protocol_h
#define m10_protocol_h

#include <stdint.h>
#include "utils.h"

/* Physical parameters */
#define M10_BAUDRATE 9600

/* Frame parameters */
#define M10_FRAME_LEN 1664

#define M10_SYNCWORD 0x66666666b366
#define M10_SYNC_LEN 6

#define M10_FTYPE_DATA 0x9F
#define M20_FTYPE_DATA 0x20

PACK(typedef struct {
	uint8_t sync_mark[3];
	uint8_t len;
	uint8_t type;
	uint8_t data[97];
	uint8_t crc[2];
}) M10Frame;


/* Specific subframe types {{{ */
PACK(typedef struct {
	uint8_t sync_mark[3];
	uint8_t len;
	uint8_t type;                       /* 0x9f */

	uint8_t small_values[2];
	uint8_t dlat[2];    /* x velocity */
	uint8_t dlon[2];    /* y velocity */
	uint8_t dalt[2];    /* z velocity */
	uint8_t time[4];    /* GPS time */
	uint8_t lat[4];
	uint8_t lon[4];
	uint8_t alt[4];
	uint8_t _pad0[6];
	uint8_t week[2];

	uint8_t _pad1[16];
	uint8_t rh_ref[3];
	uint8_t rh_counts[3];    /* ??? */
	uint8_t _pad2[6];
	uint8_t adc_temp_range;     /* Temperature range index */
	uint8_t adc_temp_val[2];    /* Temperature ADC value */
	uint8_t _pad3[4];
	uint8_t adc_batt_val[2];
	uint8_t unk3[2];    /* Correlated to adc_battery_val, also very linear */
	uint8_t _pad4[12];
	uint8_t unk4[2];    /* Fairly constant */
	uint8_t unk5[2];    /* Fairly constant */
	uint8_t unk6[2];    /* Correlated to unk0 */

	uint8_t data[10];
}) M10Frame_9f;

PACK(typedef struct {
	uint8_t sync_mark[3];
	uint8_t len;
	uint8_t type;                       /* 0x20 */
	uint8_t small_values[6];
	uint8_t alt[3];
	uint8_t dlat[2];    /* x velocity */
	uint8_t dlon[2];    /* y velocity */
	uint8_t time[3];
	uint8_t sn[3];
	uint8_t cnt[1];
	uint8_t BlkChk[2];
	uint8_t dalt[2];    /* z velocity */
	uint8_t week[2];
	uint8_t lat[4];
	uint8_t lon[4];
	uint8_t data[33];
}) M20Frame_20;
/* }}} */

#endif
