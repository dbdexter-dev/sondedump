#ifndef sondedump_data_h
#define sondedump_data_h

#include <time.h>
#include <stdint.h>

typedef enum {
	PROCEED,
	PARSED
} ParserStatus;

typedef enum {
	DATA_SEQ        = 1 << 0,       /* Sequence number */
	DATA_SERIAL     = 1 << 1,       /* Sonde serial as string */
	DATA_POS        = 1 << 2,       /* Lat/lon/alt */
	DATA_SPEED      = 1 << 3,       /* Speed/climb/heading */
	DATA_TIME       = 1 << 4,       /* Date/time */
	DATA_PTU        = 1 << 5,       /* Pressure/temperature/humidity w/ calibration percentage */
	DATA_XDATA      = 1 << 6,       /* XDATA as string */
	DATA_SHUTDOWN   = 1 << 7,       /* Auto-shutdown time */
} DataBitmask;

typedef struct {
	float o3_ppb;   /* Ozone concentration, in ppb */
} SondeXdata;

typedef struct {
	DataBitmask fields;

	int seq;            /* Packet sequence number */
	char serial[32];    /* Sonde serial number */

	float lat;          /* Latitude, degrees North */
	float lon;          /* Longitude, degrees East */
	float alt;          /* Altitude, meters */
	float speed;        /* Horizontal speed, m/s */
	float climb;        /* Climb rate, m/s up */
	float heading;      /* Heading, degrees */

	time_t time;

	uint8_t calibrated;
	float calib_percent;
	float temp;
	float rh;
	float pressure;

	SondeXdata xdata;

	int shutdown;
} SondeData;

#endif
