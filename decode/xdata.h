#ifndef xdata_h
#define xdata_h

#include <include/data.h>

#define XDATA_ENSCI_OZONE 0x05

/**
 * Decode ASCII-encoded xdata
 *
 * @param cur_pressure  current pressure, used for ozone PPM decoding
 * @param ascii_data    standard ASCII xdata string (without the 'xdata=')
 * @param len           length of the ascii_data string
 * @return decoded data
 */
void xdata_decode(SondeXdata *xdata, float cur_pressure, const char *ascii_data, int len);

#endif
