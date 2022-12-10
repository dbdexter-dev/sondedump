#ifndef sondedump_data_h
#define sondedump_data_h

#include <time.h>
#include <stdint.h>

typedef enum {
	PROCEED,
	PARSED
} ParserStatus;

typedef enum {
	DATA_DELIMITER  = 1 << 0,       /* End of frame delimiter */
	DATA_SEQ        = 1 << 1,       /* Sequence number */
	DATA_SERIAL     = 1 << 2,       /* Sonde serial as string */
	DATA_POS        = 1 << 3,       /* Lat/lon/alt */
	DATA_SPEED      = 1 << 4,       /* Speed/climb/heading */
	DATA_TIME       = 1 << 5,       /* Date/time */
	DATA_PTU        = 1 << 6,       /* Pressure/temperature/humidity w/ calibration percentage */
	DATA_XDATA      = 1 << 7,       /* XDATA as string */
	DATA_SHUTDOWN   = 1 << 8,       /* Auto-shutdown time */
} DataBitmask;

typedef struct {
	DataBitmask fields;

	int seq;
	char *serial;

	float lat, lon, alt;
	float speed, climb, heading;

	time_t time;

	uint8_t calibrated;
	float calib_percent;
	float temp, rh, pressure;

	char *xdata;

	int shutdown;
} SondeData;


#if 0
typedef enum {
	SOURCE_END,             /* Samples source exhausted, no more data left */
	FRAME_END,              /* End of this frame, start of the next */
	EMPTY,                  /* No data */
	UNKNOWN,                /* Unknown subframe format */
	INFO,                   /* Generic info (serial nr, burstkill...) */
	POSITION,               /* GPS position and velocity */
	DATETIME,               /* GPS date and time */
	PTU,                    /* Sensor data */
	XDATA,                  /* XDATA */
} DataType;

typedef struct {
	char *sonde_serial;   /* Sonde serial number */
	char *board_model;    /* Board model */
	char *board_serial;   /* Board serial number */

	int seq;                /* Frame sequence number */
	int burstkill_status;   /* -1 if not active, > 0 if active (seconds left) */
} SondeInfo;

typedef struct {
	float lat, lon, alt;
	float speed, heading, climb;
} SondePosition;

typedef struct {
	time_t datetime;
} SondeDateTime;

typedef struct {
	int calibrated;         /* 0 if uncalibrated, nonzero otherwise */
	float calib_percent;    /* Calibration percentage, 0-100 */
	float temp;             /* Temperature, degrees Celsius */
	float rh;               /* Relative humidity % */
	float pressure;         /* Pressure, mbar */
} SondePTU;

typedef struct {
	char *data;
} SondeXDATA;

typedef struct {
	DataType type;
	union {
		SondeInfo info;
		SondePosition pos;
		SondePTU ptu;
		SondeDateTime datetime;
		SondeXDATA xdata;
	} data;
} SondeData;

typedef struct {
	int seq;
	float lat, lon, alt;
	float speed, heading, climb;
	float temp, rh, pressure;
	time_t utc_time;
	int shutdown_timer;
	char serial[32];
	char xdata[128];
	int calibrated;
	float calib_percent;
} PrintableData;
#endif
#endif
