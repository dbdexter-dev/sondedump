#ifndef rs41_protocol_h
#define rs41_protocol_h

#include <stdint.h>

/* Physical parameters */
#define RS41_BAUDRATE 4800

/* Data-link parameters */
#define RS41_SYNCWORD 0x086d53884469481f
//#define RS41_SYNCWORD 0x10b6ca11229612f8
#define RS41_SYNC_LEN 8
#define RS41_RS_LEN 48
#define RS41_DATA_LEN 263
#define RS41_XDATA_LEN 198
#define RS41_MAX_FRAME_LEN (RS41_SYNC_LEN + RS41_RS_LEN + 1 + RS41_DATA_LEN + RS41_XDATA_LEN)

#define RS41_PRN_PERIOD 64
#define RS41_SUBFRAME_MAX_LEN 255 + 2   /* uint8_t max plus crc16 */

#define RS41_FLAG_EXTENDED 0xF0

#define RS41_SFTYPE_EMPTY 0x76
#define RS41_SFTYPE_INFO 0x79

#define RS41_FLIGHT_STATUS_MODE_MSK 0x1         /* 0 = preparing, 1 = in flight */
#define RS41_FLIGHT_STATUS_DESCEND_MSK 0x2      /* 0 = ascending, 1 = descending */
#define RS41_FLIGHT_STATUS_VBAT_LOW (1 << 12)   /* 0 = battery ok, 1 = battery low */

typedef struct {
	uint8_t syncword[RS41_SYNC_LEN];
	uint8_t rs_checksum[RS41_RS_LEN];
	uint8_t extended_flag;
	uint8_t data[RS41_DATA_LEN + RS41_XDATA_LEN];
} __attribute__((packed)) RS41Frame;

typedef struct {
	uint8_t type;
	uint8_t len;
	uint8_t data[RS41_SUBFRAME_MAX_LEN];
} __attribute__((packed)) RS41Subframe;

typedef struct {
	uint8_t type;
	uint8_t len;

	/* Status frame specific fields */
	uint16_t frame_seq;
	char serial[8];
	uint8_t bat_voltage;
		uint8_t _unknown[2];
	uint16_t flight_status;
		uint8_t _unknown2;
	uint8_t pcb_temp;
		uint8_t _unknown3[2];
	uint16_t humidity_heating_pwm;
	uint8_t tx_power;

	uint8_t shard_count;
	uint8_t shard_num;
	uint8_t shard_data[16];
}__attribute__((packed)) RS41Subframe_Status;

inline int rs41_frame_is_extended(RS41Frame *f) { return f->extended_flag == RS41_FLAG_EXTENDED; }
#endif
