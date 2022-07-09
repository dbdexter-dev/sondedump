#include "nuklear_ext.h"

void
nk_window_fit_to_content(struct nk_context *ctx)
{
	struct nk_panel *panel;
	struct nk_vec2 position, size;

	position = nk_window_get_position(ctx);
	size = nk_window_get_size(ctx);
	panel = nk_window_get_panel(ctx);

	if (!panel) return;

	size.y = panel->at_y - position.y + panel->row.height;

	ctx->current->bounds.h = size.y;
}

nk_bool
nk_button_colorf(struct nk_context *ctx, struct nk_colorf colorf)
{
	struct nk_color color;

	color.a = colorf.a * 255;
	color.r = colorf.r * 255;
	color.g = colorf.g * 255;
	color.b = colorf.b * 255;

	return nk_button_color(ctx, color);
}
