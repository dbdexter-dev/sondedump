#ifndef gui_style_h
#define gui_style_h

#include "nuklear/nuklear.h"

#define STYLE_DEFAULT_FONT_SIZE 17

#define STYLE_ACCENT_0 {61, 132, 224, 255}
#define STYLE_ACCENT_1 {224, 60, 131, 255}
#define STYLE_ACCENT_2 {224, 153, 60, 255}

enum font_style {
	FONT_WEIGHT_REGULAR = 0,
	FONT_WEIGHT_BOLD    = 1,
};

void gui_set_style_default(struct nk_context *ctx);
void gui_load_fonts(struct nk_context *ctx);

void gui_set_font_style(enum font_style style);

#endif
