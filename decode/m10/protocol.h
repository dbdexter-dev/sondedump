#ifndef m10_protocol_h
#define m10_protocol_h

#include <stdint.h>

/* Physical parameters */
#define M10_BAUDRATE 9600

/* Frame parameters */
#define M10_FRAME_LEN 103

#define M10_SYNCWORD 0x6666b366
#define M10_SYNCLEN 4

#define M10_FTYPE_DATA 0x9F

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
} __attribute__((packed))M10Frame_9f;
/* }}} */

inline uint32_t m10_frame_9f_time(M10Frame_9f* f) {
	return f->time[0] << 24 | f->time[1] << 16 | f->time[2] << 8 | f->time[3];
}
inline int32_t m10_frame_9f_lat(M10Frame_9f* f) {
	return f->lat[0] << 24 | f->lat[1] << 16 | f->lat[2] << 8 | f->lat[3];
}
inline int32_t m10_frame_9f_lon(M10Frame_9f* f) {
	return f->lon[0] << 24 | f->lon[1] << 16 | f->lon[2] << 8 | f->lon[3];
}
inline int32_t m10_frame_9f_alt(M10Frame_9f* f) {
	return f->alt[0] << 24 | f->alt[1] << 16 | f->alt[2] << 8 | f->alt[3];
}
inline int16_t m10_frame_9f_dlat(M10Frame_9f* f) {
	return f->dlat[0] << 8 | f->dlat[1];
}
inline int16_t m10_frame_9f_dlon(M10Frame_9f* f) {
	return f->dlon[0] << 8 | f->dlon[1];
}
inline int16_t m10_frame_9f_dalt(M10Frame_9f* f) {
	return f->dalt[0] << 8 | f->dalt[1];
}
#endif
