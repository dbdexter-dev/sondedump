#ifndef decode_h
#define decode_h

#include <include/data.h>

#define decoder_iface_t ParserStatus(*)(void*, SondeData*, const float*, size_t)

typedef struct {
	unsigned int id;
	time_t utc_time;
	float lat, lon, alt;
	float spd, hdg, climb;
	float pressure;
	float temp, rh, dewpt;
} GeoPoint;

enum decoder { AUTO=0, DFM09, IMET4, IMS100, M10, MRZN1, RS41, END};

int          decoder_init(int samplerate);
void         decoder_deinit(void);
ParserStatus decode(const float *samples, size_t len);

/**
 * Thread-safe way to change the decoder sample rate
 */
void         decoder_set_samplerate(int samplerate);

/**
 * Getter/setter for the currently active decoder
 */
enum decoder get_active_decoder(void);
void         set_active_decoder(enum decoder decoder);

/**
 * Get a pointer to the data decoded so far
 */
const SondeData* get_data(void);
int              get_slot(void);

int             get_data_count(void);
const GeoPoint* get_track_data(void);

#endif
