#include "chart.h"
#include "decode.h"
#include "utils.h"

#define HISTORY_COUNT 120

void
widget_chart(struct nk_context *ctx)
{
	const struct nk_color rh_color = {128, 0, 0, 255};
	const struct nk_color alt_color = {0, 255, 0, 255};
	const struct nk_color speed_color = {255, 128, 0, 255};
	const struct nk_color hdg_color = {128, 128, 128, 255};

	const int data_end = get_data_count();
	const int data_count = MIN(HISTORY_COUNT, data_end);
	int i;
	float *data;

	nk_layout_row_dynamic(ctx, CHART_HEIGHT, 1);

	if (nk_chart_begin(ctx, NK_CHART_LINES, data_count, -100, 100)) {
		data = get_temp_data();
		for (i=MAX(0, data_end - HISTORY_COUNT); i<data_end; i++) {
			nk_chart_push_slot(ctx, data[i], 0);
		}

		data = get_rh_data();
		nk_chart_add_slot_colored(ctx, NK_CHART_LINES, rh_color, ctx->style.chart.color, data_count, 0, 100);
		for (i=MAX(0, data_end - HISTORY_COUNT); i<data_end; i++) {
			nk_chart_push_slot(ctx, data[i], 1);
		}

		data = get_alt_data();
		nk_chart_add_slot_colored(ctx, NK_CHART_LINES, alt_color, ctx->style.chart.color, data_count, 0, 35000);
		for (i=MAX(0, data_end - HISTORY_COUNT); i<data_end; i++) {
			nk_chart_push_slot(ctx, data[i], 2);
		}

		data = get_speed_data();
		nk_chart_add_slot_colored(ctx, NK_CHART_LINES, speed_color, ctx->style.chart.color, data_count, -20, 20);
		for (i=MAX(0, data_end - HISTORY_COUNT); i<data_end; i++) {
			nk_chart_push_slot(ctx, data[i], 3);
		}

		data = get_hdg_data();
		nk_chart_add_slot_colored(ctx, NK_CHART_LINES, hdg_color, ctx->style.chart.color, data_count, 0, 360);
		for (i=MAX(0, data_end - HISTORY_COUNT); i<data_end; i++) {
			nk_chart_push_slot(ctx, data[i], 4);
		}

		nk_chart_end(ctx);
	}
}
