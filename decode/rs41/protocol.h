#ifndef rs41_protocol_h
#define rs41_protocol_h

#include <stdint.h>

/* Physical parameters */
#define RS41_BAUDRATE 4800

/* Frame parameters */
#define RS41_SYNCWORD 0x086d53884469481f
//#define RS41_SYNCWORD 0x10b6ca11229612f8
#define RS41_SYNC_LEN 8
#define RS41_RS_LEN 48
#define RS41_DATA_LEN 263
#define RS41_XDATA_LEN 198
#define RS41_MAX_FRAME_LEN (RS41_SYNC_LEN + RS41_RS_LEN + 1 + RS41_DATA_LEN + RS41_XDATA_LEN)

#define RS41_PRN_PERIOD 64
#define RS41_FLAG_EXTENDED 0xF0

/* Reed-Solomon ECC parameters, found by bruteforcing the RS code on a known
 * good frame */
#define RS41_REEDSOLOMON_INTERLEAVING 2
#define RS41_REEDSOLOMON_N 255
#define RS41_REEDSOLOMON_K 231
#define RS41_REEDSOLOMON_T (RS41_REEDSOLOMON_N - RS41_REEDSOLOMON_K)
#define RS41_REEDSOLOMON_POLY 0x11D
#define RS41_REEDSOLOMON_FIRST_ROOT 0x00
#define RS41_REEDSOLOMON_ROOT_SKIP 1

/* Subframe parameters */
#define RS41_SUBFRAME_MAX_LEN 255 + 2   /* uint8_t max plus crc16 */

#define RS41_SFTYPE_EMPTY 0x76
#define RS41_SFTYPE_INFO 0x79
#define RS41_SFTYPE_PTU 0x7A
#define RS41_SFTYPE_GPSPOS 0x7B
#define RS41_SFTYPE_GPSINFO 0x7C
#define RS41_SFTYPE_GPSRAW 0x7D
#define RS41_SFTYPE_XDATA 0x7E

#define RS41_CALIB_FRAGSIZE 16

#define RS41_SERIAL_LEN 8

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

/* Specific subframe types {{{ */
typedef struct {
	uint8_t type;
	uint8_t len;

	/* Status frame specific fields */
	uint16_t frame_seq;
	char serial[RS41_SERIAL_LEN];
	uint8_t bat_voltage;
	uint8_t _unknown[2];
	uint16_t flight_status;
	uint8_t _unknown2;
	uint8_t pcb_temp;
	uint8_t _unknown3[2];
	uint16_t humidity_heating_pwm;
	uint8_t tx_power;

	uint8_t frag_count;
	uint8_t frag_seq;
	uint8_t frag_data[16];
} __attribute__((packed)) RS41Subframe_Status;

typedef struct {
	uint8_t type;
	uint8_t len;

	/* PTU frame specific fields */
	uint8_t temp_main[3];
	uint8_t temp_ref1[3];
	uint8_t temp_ref2[3];

	uint8_t humidity_main[3];
	uint8_t humidity_ref1[3];
	uint8_t humidity_ref2[3];

	uint8_t temp_humidity_main[3];
	uint8_t temp_humidity_ref1[3];
	uint8_t temp_humidity_ref2[3];

	uint8_t pressure_main[3];
	uint8_t pressure_ref1[3];
	uint8_t pressure_ref2[3];

	uint8_t _zero[3];

	int16_t pressure_temp;

	uint8_t _zero2[3];
} __attribute__((packed)) RS41Subframe_PTU;

typedef struct {
	uint8_t type;
	uint8_t len;

	/* GPS position specific fields */
	int32_t x;
	int32_t y;
	int32_t z;
	int16_t dx;
	int16_t dy;
	int16_t dz;
	uint8_t sv_count;
	uint8_t speed_acc_estimate;
	uint8_t pdop;
} __attribute__((packed)) RS41Subframe_GPSPos;

typedef struct {
	uint8_t type;
	uint8_t len;

	/* GPS position specific fields */
	uint16_t week;
	uint32_t ms;
	uint16_t satellite[12];
} __attribute__((packed)) RS41Subframe_GPSInfo;

typedef struct {
	uint8_t type;
	uint8_t len;
	uint8_t unknown;
	char ascii_data[RS41_XDATA_LEN];
}__attribute__((packed)) RS41Subframe_XDATA;

/* }}} */

/* Credits to @einergehtnochrein (https://github.com/einergehtnochrein/ra-firmware)
 * for figuring out what each calibration field does */
typedef struct {
	uint8_t _pad0[13];
	char sonde_serial[8];       /* Sonde serial number, ASCII */
	uint8_t _pad1[40];

	float t_ref[2];            /* Value of the temperature ref. resistances (ohm) */
	float rh_ref[2];           /* Value of the rel. hum. capacitance reference values (pF) */
	float t_temp_poly[3];      /* Resistance -> temp 2nd degree polynomial */
	float t_calib_coeff[7];    /* Resistance correction coefficients */
	float rh_cap_calib[2];
	float rh_calib_coeff[7][6];
	float th_temp_poly[3];      /* Resistance -> temp humidity 2nd degree poly */
	float th_calib_coeff[7];   /* Resistance correction coefficients */

	uint8_t _midpad[273];

	float p_calib_coeff[18];
	float vector_bp[3];
	uint8_t _pad4[8];
	float matrix_bt[12];
	uint8_t _pad5[54];
	uint16_t burstkill_timer;   /* Time to shutdown after balloon burst (or maybe just time to shutdown in general) */
	uint8_t _pad6[6];
	uint8_t _unk_dynamic0[4];   /* Unknown, counter-like */
	uint8_t _unk_dynamic1[2];   /* Unknown, last byte probably flags */


} __attribute__((packed)) RS41Calibration;

#endif
