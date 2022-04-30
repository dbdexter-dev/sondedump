#include "decode.h"
#include "menubar.h"
#include "utils.h"
#include "gui.h"

extern const char *_decoder_names[];
extern const int _decoder_count;

const char *view_types[] = {"Raw plot", "Skew-T diagram", "OpenStreetMap"};

void
widget_menubar(struct nk_context *ctx, int width, int height)
{
	(void)width;
	int i;
	int active_decoder;
	enum nk_symbol_type menu_symbol;

	active_decoder = get_active_decoder();

	nk_menubar_begin(ctx);
	nk_layout_row_begin(ctx, NK_STATIC, MENU_ITEM_HEIGHT, 4);

	/* File */
	nk_layout_row_push(ctx, 40);
	if (nk_menu_begin_label(ctx, "File", NK_TEXT_ALIGN_CENTERED, nk_vec2(100, height))) {
		nk_layout_row_dynamic(ctx, MENU_ITEM_HEIGHT, 1);
		nk_menu_item_label(ctx, "Open", NK_TEXT_LEFT);
		nk_menu_item_label(ctx, "Start", NK_TEXT_LEFT);
		nk_menu_end(ctx);
	}

	/* Decode */
	nk_layout_row_push(ctx, 50);
	if (nk_menu_begin_label(ctx, "Decode", NK_TEXT_ALIGN_CENTERED, nk_vec2(120, height))) {
		nk_layout_row_dynamic(ctx, MENU_ITEM_HEIGHT, 1);
		for (i=0; i<_decoder_count; i++) {
			menu_symbol = i == active_decoder ? NK_SYMBOL_CIRCLE_SOLID : NK_SYMBOL_CIRCLE_OUTLINE;
			if (nk_menu_item_symbol_label(ctx, menu_symbol, _decoder_names[i], NK_TEXT_RIGHT)) {
				set_active_decoder(i);
			}
		}
		nk_menu_end(ctx);
	}

	/* View */
	nk_layout_row_push(ctx, 50);
	if (nk_menu_begin_label(ctx, "View", NK_TEXT_ALIGN_CENTERED, nk_vec2(150, height))) {
		nk_layout_row_dynamic(ctx, MENU_ITEM_HEIGHT, 1);
		for (i=0; i<(int)LEN(view_types); i++) {
			if (nk_menu_item_label(ctx, view_types[i], NK_TEXT_LEFT)) {
				gui_set_graph(i);
			}
		}
		nk_menu_end(ctx);
	}

	/* Help */
	nk_layout_row_push(ctx, 40);
	if (nk_menu_begin_label(ctx, "Help", NK_TEXT_ALIGN_CENTERED, nk_vec2(100, height))) {
		nk_layout_row_dynamic(ctx, MENU_ITEM_HEIGHT, 1);
		nk_menu_item_label(ctx, "About", NK_TEXT_LEFT);
		nk_menu_end(ctx);
	}

	nk_layout_row_end(ctx);
	nk_menubar_end(ctx);
}
