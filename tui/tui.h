#ifndef tui_h
#define tui_h

#include "decode/common.h"

void tui_init(int update_interval, void (*decoder_changer)(int delta), int active_decoder);
void tui_deinit();

int tui_update(SondeData *data);
#endif

