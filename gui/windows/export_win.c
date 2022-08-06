#include <stdlib.h>
#include "export_win.h"
#include "gui/nuklear/nuklear_ext.h"
#include "gui/widgets/export.h"

void
export_window(struct nk_context *ctx, UIState *state, Config *config, float width, float height)
{
	const char *title = "Export";
	const enum nk_panel_flags win_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR
	                                    | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE;

	if (nk_begin(ctx, title, nk_rect(width/2, height/2, PANEL_WIDTH * config->ui_scale, 0), win_flags)) {
		widget_export(ctx, config->ui_scale);

		nk_window_fit_to_content(ctx);
		state->over_window |= nk_window_is_hovered(ctx);
	} else {
		state->export_open = 0;
	}

	nk_end(ctx);
}
