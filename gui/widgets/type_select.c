#include "decode.h"
#include "style.h"
#include "type_select.h"

#define TEXT_WIDTH 90

extern const char *_decoder_names[];
extern const int _decoder_count;

void
widget_type_select(struct nk_context *ctx, float scale)
{
	struct nk_panel *window_panel;
	int border;
	struct nk_rect bounds, inner_bounds;
	int selected, prev_selected;

	prev_selected = selected = get_active_decoder();
	window_panel = nk_window_get_panel(ctx);
	border = window_panel->border;

	/* Sonde type selection */
	nk_layout_row_begin(ctx, NK_STATIC, STYLE_DEFAULT_ROW_HEIGHT * scale, 2);
	nk_layout_row_push(ctx, TEXT_WIDTH * scale);
	nk_label(ctx, "Type:", NK_TEXT_RIGHT);
	bounds = nk_layout_widget_bounds(ctx);
	nk_layout_row_push(ctx, bounds.w - TEXT_WIDTH*scale - 2*border);
	inner_bounds = nk_widget_bounds(ctx);
	nk_combobox(ctx,
	            _decoder_names,
	            _decoder_count,
	            &selected,
	            STYLE_DEFAULT_ROW_HEIGHT * scale,
	            nk_vec2(inner_bounds.w, _decoder_count * bounds.h + 10*scale)
	);

	/* Handle selection change */
	if (prev_selected != selected) {
		set_active_decoder(selected);
	}
	nk_layout_row_end(ctx);
}
