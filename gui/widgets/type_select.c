#include "decode.h"
#include "type_select.h"

#define DATA_ITEM_HEIGHT 20
#define TEXT_WIDTH 90

extern const char *_decoder_names[];
extern const int _decoder_count;

void
widget_type_select(struct nk_context *ctx)
{
	struct nk_rect bounds, inner_bounds;
	int selected, prev_selected;

	prev_selected = selected = get_active_decoder();

	/* Sonde type selection */
	nk_layout_row_begin(ctx, NK_STATIC, DATA_ITEM_HEIGHT, 2);
	nk_layout_row_push(ctx, TEXT_WIDTH);
	nk_label(ctx, "Type:", NK_TEXT_RIGHT);
	bounds = nk_layout_widget_bounds(ctx);
	nk_layout_row_push(ctx, bounds.w - TEXT_WIDTH - 4);
	inner_bounds = nk_widget_bounds(ctx);
	nk_combobox(ctx,
	            _decoder_names,
	            _decoder_count,
	            &selected,
	            DATA_ITEM_HEIGHT,
	            nk_vec2(inner_bounds.w, _decoder_count * (bounds.h + 1))
	);

	/* Handle selection change */
	if (prev_selected != selected) {
		set_active_decoder(selected);
	}
}
