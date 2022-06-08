#ifndef decode_h
#define decode_h

#include <include/data.h>

#define decoder_iface_t ParserStatus(*)(void*, SondeData*, const float*, size_t)

typedef struct {
	float lat, lon, alt;
	float speed, hdg;
	float temp, rh;
} GeoPoint;

enum decoder { AUTO=0, DFM09, IMET4, IMS100, M10, RS41, END};

int          decoder_init(int samplerate);
void         decoder_deinit(void);
ParserStatus decode(const float *samples, size_t len);

enum decoder get_active_decoder(void);
void         set_active_decoder(enum decoder decoder);

PrintableData *get_data(void);
int            get_slot(void);

int             get_data_count(void);
const GeoPoint* get_track_data(void);


#endif
