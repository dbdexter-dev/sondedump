#ifndef imet4_protocol_h
#define imet4_protocol_h

#include <stdint.h>
#include "utils.h"

#define IMET4_BAUDRATE 1200
#define IMET4_SYNCWORD 0xFFFFFF40
#define IMET4_SYNC_LEN 4
#define IMET4_MARK_FREQ 2200.0
#define IMET4_SPACE_FREQ 1200.0

#define IMET4_FRAME_LEN 600


enum imet4_sftype {
	IMET4_SFTYPE_PTU = 0x01,
	IMET4_SFTYPE_GPS = 0x02,
	IMET4_SFTYPE_XDATA = 0x03,
	IMET4_SFTYPE_PTUX = 0x04,
	IMET4_SFTYPE_GPSX = 0x05,
};

enum imet4_xdata_instr {
	IMET4_XDATA_INSTR_OZONE = 0x01,
	IMET4_XDATA_INSTR_HYGRO = 0x10,
};

PACK(typedef struct {
	uint8_t sync[3];
	uint8_t data[IMET4_FRAME_LEN/8 - 3];
}) IMET4Frame;

PACK(typedef struct {
	uint8_t soh;
	uint8_t type;
	uint8_t data[IMET4_FRAME_LEN/8 - 5];
}) IMET4Subframe;

/* Specific subframe types {{{ */
PACK(typedef struct {
	uint8_t soh;
	uint8_t type;
	uint16_t seq;
	uint8_t pressure[3];
	int16_t temp;
	int16_t rh;
	uint8_t vbat;
}) IMET4Subframe_PTU;

PACK(typedef struct {
	uint8_t soh;
	uint8_t type;
	uint16_t seq;
	uint8_t pressure[3];
	int16_t temp;
	int16_t rh;
	uint8_t vbat;
	int16_t temp_internal;
	int16_t temp_pressure;
	int16_t temp_humidity;
}) IMET4Subframe_PTUX;

PACK(typedef struct {
	uint8_t soh;
	uint8_t type;
	float lat;
	float lon;
	int16_t alt;
	uint8_t num_sats;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
}) IMET4Subframe_GPS;

PACK(typedef struct {
	uint8_t soh;
	uint8_t type;
	float lat;
	float lon;
	int16_t alt;
	uint8_t num_sats;
	float dlon;
	float dlat;
	float climb;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
}) IMET4Subframe_GPSX;

PACK(typedef struct {
	uint8_t soh;
	uint8_t type;
	uint8_t len;
	uint8_t instr_id;
	uint8_t dc_idx;
	uint8_t data;   /* Variable length, just a pointer to the first element */
}) IMET4Subframe_XDATA;
/* }}} */
/* XDATA sub-subframe types {{{ */
PACK(typedef struct {
	uint8_t cell_current[2];
	uint8_t pump_temp[2];
	uint8_t pump_current;
	uint8_t vbatt;
}) IMET4Subframe_XDATA_Ozone;
/* }}} */

#endif
