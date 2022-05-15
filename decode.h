#ifndef decode_h
#define decode_h

#include <include/data.h>

#define decoder_iface_t ParserStatus(*)(void*, SondeData*, const float*, size_t)

enum decoder { AUTO=0, DFM09, IMET4, IMS100, M10, RS41, END};

void         decoder_init(int samplerate);
void         decoder_deinit(void);
ParserStatus decode(const float *samples, size_t len);

enum decoder get_active_decoder(void);
void         set_active_decoder(enum decoder decoder);

PrintableData *get_data(void);
int            get_slot(void);

int    get_data_count(void);
const float *get_temp_data(void);
const float *get_rh_data(void);
const float *get_hdg_data(void);
const float *get_speed_data(void);
const float *get_alt_data(void);
const float *get_lat_data(void);
const float *get_lon_data(void);


#endif
