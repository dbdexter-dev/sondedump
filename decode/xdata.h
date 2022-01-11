#ifndef xdata_h
#define xdata_h

#define XDATA_ENSCI_OZONE 0x05

/**
 * Decoder ASCII-encoded xdata
 *
 * @param cur_pressure current pressure, used for ozone PPM decoding
 * @param ascii_data standard ASCII xdata string (without the 'xdata=')
 * @param len length of the ascii_data string
 * @return decoded data
 */
char *xdata_decode(float cur_pressure, const char *ascii_data, int len);

#endif
