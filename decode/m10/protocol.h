#ifndef m10_protocol_h
#define m10_protocol_h

#include <stdint.h>
#include <time.h>

/* Physical parameters */
#define M10_BAUDRATE 9600

/* Frame parameters */
#define M10_FRAME_LEN 103

#define M10_SYNCWORD 0x6666b366
#define M10_SYNCLEN 4

#define M10_FTYPE_DATA 0x9F
#define M20_FTYPE_DATA 0x20

typedef struct {
	uint8_t sync_mark[3];
	uint8_t type;
	uint8_t data[M10_FRAME_LEN-6];
	uint8_t crc[2];
} __attribute__((packed)) M10Frame;


/* Specific subframe types {{{ */
typedef struct {
	uint8_t sync_mark[3];
	uint8_t type;                       /* 0x9f */

	uint8_t small_values[2];
	uint8_t dlat[2], dlon[2], dalt[2];  /* x, y, z velocity */
	uint8_t time[4];                    /* GPS time */
	uint8_t lat[4], lon[4], alt[4];     /* Latitude, longitude, altitude */
	uint8_t _pad0[6];
	uint8_t week[2];

	uint8_t data[80];
} __attribute__((packed))M10Frame_9f;
/* }}} */

time_t m10_frame_9f_time(M10Frame_9f* f);
void m10_frame_9f_serial(char *dst, M10Frame_9f *frame);

inline float m10_frame_9f_lat(M10Frame_9f* f) {
	int32_t lat =  f->lat[0] << 24 | f->lat[1] << 16 | f->lat[2] << 8 | f->lat[3];
	return lat * 360.0 / (1UL << 32);
}
inline float m10_frame_9f_lon(M10Frame_9f* f) {
	int32_t lon = f->lon[0] << 24 | f->lon[1] << 16 | f->lon[2] << 8 | f->lon[3];
	return lon * 360.0 / (1UL << 32);
}
inline float m10_frame_9f_alt(M10Frame_9f* f) {
	int32_t alt =  f->alt[0] << 24 | f->alt[1] << 16 | f->alt[2] << 8 | f->alt[3];
	return alt / 1e3;
}
inline float m10_frame_9f_dlat(M10Frame_9f* f) {
	int16_t dlat =  f->dlat[0] << 8 | f->dlat[1];
	return dlat / 200.0;
}
inline float m10_frame_9f_dlon(M10Frame_9f* f) {
	int16_t dlon =  f->dlon[0] << 8 | f->dlon[1];
	return dlon / 200.0;
}
inline float m10_frame_9f_dalt(M10Frame_9f* f) {
	int16_t dalt = f->dalt[0] << 8 | f->dalt[1];
	return dalt / 200.0;
}
#endif
