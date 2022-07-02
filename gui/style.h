#ifndef gui_style_h
#define gui_style_h

#include "nuklear/nuklear.h"

#define STYLE_DEFAULT_FONT_SIZE 17
#define STYLE_DEFAULT_ROW_HEIGHT (STYLE_DEFAULT_FONT_SIZE + 4)

#define STYLE_ACCENT_0 {61, 132, 224, 255}
#define STYLE_ACCENT_1 {224, 60, 131, 255}
#define STYLE_ACCENT_2 {224, 153, 60, 255}
#define STYLE_ACCENT_3 {132, 224, 61, 255}

#define STYLE_ACCENT_0_NORMALIZED {61/255.0, 132/255.0, 224/255.0, 1}
#define STYLE_ACCENT_1_NORMALIZED {224/255.0, 60/255.0, 131/255.0, 1}
#define STYLE_ACCENT_2_NORMALIZED {224/255.0, 153/255.0, 60/255.0, 1}
#define STYLE_ACCENT_3_NORMALIZED {132/255.0, 224/255.0, 61/255.0, 1}

/**
 * Initialize the default colorscheme to something more visually appealing than
 * the one shipped with the library
 */
void gui_set_style_default(struct nk_context *ctx);

/**
 * (re)load fonts and bake them into a texture
 */
void gui_load_fonts(struct nk_context *ctx, float scale);

#endif
