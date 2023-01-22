#ifndef mrzn1_parser_h
#define mrzn1_parser_h

#include <time.h>
#include "protocol.h"

int mrzn1_seq(const MRZN1Frame *frame);
int mrzn1_calib_seq(const MRZN1Frame *frame);
time_t mrzn1_time(const MRZN1Frame *frame, const MRZN1Calibration *calib);
float mrzn1_x(const MRZN1Frame *frame);
float mrzn1_y(const MRZN1Frame *frame);
float mrzn1_z(const MRZN1Frame *frame);
float mrzn1_dx(const MRZN1Frame *frame);
float mrzn1_dy(const MRZN1Frame *frame);
float mrzn1_dz(const MRZN1Frame *frame);

float mrzn1_temp(const MRZN1Frame *frame);
float mrzn1_rh(const MRZN1Frame *frame);

void mrzn1_serial(char *dst, const MRZN1Calibration *calib);

#endif
