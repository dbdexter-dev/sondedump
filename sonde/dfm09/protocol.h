#ifndef dfm09_protocol_h
#define dfm09_protocol_h

#include <stdint.h>

#define DFM09_BAUDRATE 2500

#define DFM09_SYNCWORD 0x9a995a55
#define DFM09_SYNC_LEN 4
#define DFM09_FRAME_LEN 560

/* Interleaving parameters */
#define DFM09_INTERLEAVING_PTU 7
#define DFM09_INTERLEAVING_GPS 13

#define DFM06_SERIAL_TYPE 6

typedef struct {
	uint8_t sync[2];
	uint8_t ptu[7];
	uint8_t gps[26];
} __attribute__((packed)) DFM09ECCFrame;

typedef struct {
	uint8_t type;
	uint8_t data[6];
} DFM09Subframe_GPS;

typedef struct {
	uint8_t type;
	uint8_t data[3];
} DFM09Subframe_PTU;

typedef struct {
	DFM09Subframe_PTU ptu;
	DFM09Subframe_GPS gps[2];
} DFM09Frame;

typedef struct {
	uint32_t raw[0x10];
} __attribute__((packed)) DFM09Calib;
#endif
