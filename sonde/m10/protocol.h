#ifndef m10_protocol_h
#define m10_protocol_h

#include <stdint.h>
#include "utils.h"

/* Physical parameters */
#define M10_BAUDRATE 9600

/* Frame parameters */
#define M10_FRAME_LEN 1664

#define M10_SYNCWORD 0x66666666b366
#define M10_SYNC_LEN 48
#define M10_MAX_DATA_LEN 99

#define M10_FTYPE_DATA 0x9F
#define M20_FTYPE_DATA 0x20

PACK(typedef struct {
	uint8_t sync_mark[3];
	uint8_t len;
	uint8_t type;
	uint8_t data[M10_MAX_DATA_LEN];
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
	uint8_t _pad0[4];
	uint8_t sat_count;   /* Number of satellites used for fix */
	uint8_t _pad3;
	uint8_t week[2];    /* GPS week */

	uint8_t prn[12];            /* PRNs of satellites used for fix */
	uint8_t _pad1[4];
	uint8_t rh_ref[3];          /* RH reading @ 55% */
	uint8_t rh_counts[3];       /* RH reading */
	uint8_t _pad2[6];
	uint8_t adc_temp_range;     /* Temperature range index */
	uint8_t adc_temp_val[2];    /* Temperature ADC value */
	uint8_t unk0[4];            /* Probably related to temp range */
	uint8_t adc_batt_val[2];
	uint8_t unk3[2];    /* Correlated to adc_battery_val, also very linear */
	uint8_t _pad4[12];
	uint8_t unk4[2];    /* Fairly constant */
	uint8_t unk5[2];    /* Fairly constant */
	uint8_t unk6[2];    /* Correlated to unk0 */
	uint8_t unk7[2];

	uint8_t serial[5];
	uint8_t seq;
}) M10Frame_9f;

PACK(typedef struct {
	uint8_t sync_mark[3];
	uint8_t len;
	uint8_t type;           /* 0x20 */
	uint8_t rh_counts[2];
	uint8_t adc_temp[2];    /* Temperature range + ADC value */
	uint8_t adc_rh_temp[2];
	uint8_t alt[3];
	uint8_t dlat[2];    /* x velocity */
	uint8_t dlon[2];    /* y velocity */
	uint8_t time[3];
	uint8_t sn[3];
	uint8_t seq;
	uint8_t BlkChk[2];
	uint8_t dalt[2];    /* z velocity */
	uint8_t week[2];
	uint8_t lat[4];
	uint8_t lon[4];
	uint8_t unk1[11];
	uint8_t rh_ref[2];
	uint8_t data[32];
}) M20Frame_20;
/* }}} */

#endif
