#ifndef dfm09_protocol_h
#define dfm09_protocol_h

#include <stdint.h>
#include "utils.h"

#define DFM09_BAUDRATE 2500

#define DFM09_SYNCWORD 0x9a995a55
#define DFM09_SYNC_LEN 32
#define DFM09_FRAME_LEN 560

/* Interleaving parameters */
#define DFM09_INTERLEAVING_PTU 7
#define DFM09_INTERLEAVING_GPS 13

#define DFM06_SERIAL_TYPE 0x06

enum dfm_gps_sftype {
	DFM_SFTYPE_SEQ = 0x00,
	DFM_SFTYPE_TIME = 0x01,
	DFM_SFTYPE_LAT = 0x02,
	DFM_SFTYPE_LON = 0x03,
	DFM_SFTYPE_ALT = 0x04,
	DFM_SFTYPE_DATE = 0x08,
};

enum dfm_ptu_sftype {
	DFM_SFTYPE_TEMP = 0x00,
	DFM_SFTYPE_RH = 0x01,
};

PACK(typedef struct {
	uint8_t sync[2];
	uint8_t ptu[7];
	uint8_t gps[26];
}) DFM09ECCFrame;

PACK(typedef struct {
	uint8_t type;
	uint8_t data[6];
}) DFM09Subframe_GPS;

PACK(typedef struct {
	uint8_t type;
	uint8_t data[3];
}) DFM09Subframe_PTU;

PACK(typedef struct {
	DFM09Subframe_PTU ptu;
	DFM09Subframe_GPS gps[2];
}) DFM09Frame;

PACK(typedef struct {
	uint32_t raw[0x10];
}) DFM09Calib;
#endif
