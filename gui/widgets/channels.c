#include "channels.h"

void
widget_plot_channels(struct nk_context *ctx, int width, int height)
{
	nk_chart_begin(ctx, NK_CHART_LINES, 3, -100, 100);

	nk_chart_end(ctx);
}
