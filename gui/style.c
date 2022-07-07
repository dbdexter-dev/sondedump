#include "style.h"
#include "nuklear/nuklear_sdl_gl3.h"
#include "utils.h"

#define NK_COLOR_MAP(NK_COLOR) \
    NK_COLOR(NK_COLOR_TEXT,                     255,255,255,255)\
    NK_COLOR(NK_COLOR_WINDOW,                   17, 17, 17, 255)\
    NK_COLOR(NK_COLOR_HEADER,                   40, 40, 40, 255)\
    NK_COLOR(NK_COLOR_BORDER,                   40, 40, 40, 255)\
    NK_COLOR(NK_COLOR_BUTTON,                   55, 55, 55, 255)\
    NK_COLOR(NK_COLOR_BUTTON_HOVER,             60, 60, 60, 255)\
    NK_COLOR(NK_COLOR_BUTTON_ACTIVE,            90, 90, 90, 255)\
    NK_COLOR(NK_COLOR_TOGGLE,                   60, 60, 60, 255)\
    NK_COLOR(NK_COLOR_TOGGLE_HOVER,             90, 90, 90, 255)\
    NK_COLOR(NK_COLOR_TOGGLE_CURSOR,            61, 132,224,255)\
    NK_COLOR(NK_COLOR_SELECT,                   45, 45, 45, 255)\
    NK_COLOR(NK_COLOR_SELECT_ACTIVE,            61, 132,224,255)\
    NK_COLOR(NK_COLOR_SLIDER,                   38, 38, 38, 255)\
    NK_COLOR(NK_COLOR_SLIDER_CURSOR,            61, 132,224,255)\
    NK_COLOR(NK_COLOR_SLIDER_CURSOR_HOVER,      120,120,120,255)\
    NK_COLOR(NK_COLOR_SLIDER_CURSOR_ACTIVE,     150,150,150,255)\
    NK_COLOR(NK_COLOR_PROPERTY,                 38, 38, 38, 255)\
    NK_COLOR(NK_COLOR_EDIT,                     38, 38, 38, 255)\
    NK_COLOR(NK_COLOR_EDIT_CURSOR,              175,175,175,255)\
    NK_COLOR(NK_COLOR_COMBO,                    45, 45, 45, 255)\
    NK_COLOR(NK_COLOR_CHART,                    23, 23, 23, 255)\
    NK_COLOR(NK_COLOR_CHART_COLOR,              61,132,224, 255)\
    NK_COLOR(NK_COLOR_CHART_COLOR_HIGHLIGHT,    255, 0,  0, 255)\
    NK_COLOR(NK_COLOR_SCROLLBAR,                40, 40, 40, 255)\
    NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR,         100,100,100,255)\
    NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR_HOVER,   120,120,120,255)\
    NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR_ACTIVE,  150,150,150,255)\
    NK_COLOR(NK_COLOR_TAB_HEADER,               40, 40, 40, 255)


extern const char _binary_Roboto_Medium_ttf[];
extern const unsigned long _binary_Roboto_Medium_ttf_size;

#define NK_COLOR(n,r,g,b,a) {r,g,b,a},
static const struct nk_color default_style_table[NK_COLOR_COUNT] = {
	NK_COLOR_MAP(NK_COLOR)
};
#undef NK_COLOR

static struct nk_font *latin_regular;

void
gui_set_style_default(struct nk_context *ctx)
{
	nk_style_from_table(ctx, default_style_table);
}

void
gui_load_fonts(struct nk_context *ctx, float scale)
{

	struct nk_font_atlas *atlas_latin_regular;
	struct nk_font_config cfg_latin_regular;

	cfg_latin_regular = nk_font_config(0);
	cfg_latin_regular.range = nk_font_default_glyph_ranges();
	cfg_latin_regular.oversample_h = cfg_latin_regular.oversample_v = 1;
	cfg_latin_regular.pixel_snap = 1;

	/* Bake latin symbols */
	nk_sdl_font_stash_begin(&atlas_latin_regular);
	latin_regular = nk_font_atlas_add_from_memory(
			atlas_latin_regular,
			(void*)_binary_Roboto_Medium_ttf,
			(size_t)SYMSIZE(_binary_Roboto_Medium_ttf),
			STYLE_DEFAULT_FONT_SIZE * scale,
			&cfg_latin_regular);
	nk_sdl_font_stash_end();

	/* Set font */
	nk_style_set_font(ctx, &latin_regular->handle);
}
