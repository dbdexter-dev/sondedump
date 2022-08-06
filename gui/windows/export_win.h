#ifndef export_window_h
#define export_window_h

#include "config.h"
#include "gui/nuklear/nuklear.h"
#include "gui/state.h"

void export_window(struct nk_context *ctx, UIState *state, Config *conf, float width, float height);

#endif
