#ifndef common_h
#define common_h

#include <stdint.h>

typedef enum {
	SOURCE_END,             /* Samples source exhausted, no more data left */
	FRAME_END,              /* End of this frame, start of the next */
	EMPTY,                  /* No data */
	UNKNOWN,                /* Unknown subframe format */
	INFO,                   /* Generic info (serial nr, burstkill...) */
	POSITION,               /* GPS position and velocity */
	PTU,                    /* Sensor data */
} DataType;

typedef struct {
	char *sonde_serial;   /* Sonde serial number */
	char *board_model;    /* Board model */
	char *board_serial;   /* Board serial number */

	int seq;                /* Frame sequence number */
	int burstkill_status;   /* -1 if not active, > 0 if active (seconds left) */
} SondeInfo;

typedef struct {
	float x, y, z;          /* ECEF coordinates */
	float dx, dy, dz;       /* ECEF velocity vector */
} SondePosition;

typedef struct {
	int calibrated;         /* 0 if uncalibrated, nonzero otherwise */
	float temp;             /* Temperature, degrees Celsius */
	float rh;               /* Relative humidity % */
	float pressure;         /* Pressure, mbar */
} SondePTU;

typedef struct {
	DataType type;
	union {
		SondeInfo info;
		SondePosition pos;
		SondePTU ptu;
	} data;
} SondeData;


#endif
