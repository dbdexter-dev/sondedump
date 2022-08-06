#ifndef config_window_h
#define config_window_h

#include "config.h"
#include "gui/nuklear/nuklear.h"
#include "gui/state.h"

void config_window(struct nk_context *ctx, UIState *state, Config *conf, float width, float height);

#endif
