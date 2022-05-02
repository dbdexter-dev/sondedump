#include "chart.h"
#include "decode.h"
#include "utils.h"

#define HISTORY_COUNT 240

void
widget_chart(struct nk_context *ctx)
{
	const struct nk_color temp_color = {192, 64, 0, 255};
	const struct nk_color rh_color = {0, 128, 255, 255};
	const struct nk_color alt_color = {0, 128, 64, 255};

	const int data_end = get_data_count();
	const int data_count = MIN(HISTORY_COUNT, data_end);
	struct nk_rect bounds;
	float chart_size, data_step;
	float i;
	float *data;

	data_step = MAX(1, data_end / (float)HISTORY_COUNT);

	bounds = nk_layout_widget_bounds(ctx);
	chart_size = nk_window_get_height(ctx) - bounds.y - 10;
	nk_layout_row_static(ctx, chart_size, chart_size, 1);


	if (nk_chart_begin(ctx, chart_size, data_count, -100, 100)) {
		data = get_temp_data();
		nk_chart_add_slot_colored(ctx, NK_CHART_LINES, temp_color, ctx->style.chart.color, data_count, -100, 100);
		for (i=0; i<data_end; i+=data_step) {
			nk_chart_push_slot(ctx, data[(int)i], 1);
		}

		data = get_rh_data();
		nk_chart_add_slot_colored(ctx, NK_CHART_LINES, rh_color, ctx->style.chart.color, data_count, 0, 100);
		for (i=0; i<data_end; i+=data_step) {
			nk_chart_push_slot(ctx, data[(int)i], 2);
		}

		data = get_alt_data();
		nk_chart_add_slot_colored(ctx, NK_CHART_LINES, alt_color, ctx->style.chart.color, data_count, 0, 35000);
		for (i=0; i<data_end; i+=data_step) {
			nk_chart_push_slot(ctx, data[(int)i], 3);
		}

		nk_chart_end(ctx);
	}
}

void
widget_skew_t(struct nk_context *ctx)
{
}
