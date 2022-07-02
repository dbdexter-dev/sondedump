#ifndef decode_h
#define decode_h

#include <include/data.h>

#define decoder_iface_t ParserStatus(*)(void*, SondeData*, const float*, size_t)

<<<<<<< HEAD
=======
typedef struct {
	unsigned int id;
	float lat, lon, alt;
	float pressure;
	float temp, rh, dewpt;
} GeoPoint;

>>>>>>> 6d1235f (Simple logger + comments)
enum decoder { AUTO=0, DFM09, IMET4, IMS100, M10, RS41, END};

void         decoder_init(int samplerate);
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
PrintableData *get_data(void);
int            get_slot(void);

#endif
