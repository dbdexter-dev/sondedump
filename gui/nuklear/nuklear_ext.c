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
