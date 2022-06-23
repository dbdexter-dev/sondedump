#ifndef NDEBUG
#include <stdio.h>
#endif
#include "gui/style.h"
#include "io/audio.h"
#include "audio_dev_select.h"
#include "decode.h"

static int _selected_idx = 0;

#define TEXT_WIDTH 90

void
widget_audio_dev_select(struct nk_context *ctx)
{
	const int num_devices = audio_get_num_devices();
	const char **dev_names = audio_get_device_names();
	struct nk_rect bounds, inner_bounds;
	int prev_selected = _selected_idx;
	int samplerate;

	nk_layout_row_begin(ctx, NK_STATIC, STYLE_DEFAULT_ROW_HEIGHT, 2);
	nk_layout_row_push(ctx, TEXT_WIDTH);
	nk_label(ctx, "Input device:", NK_TEXT_RIGHT);
	bounds = nk_layout_widget_bounds(ctx);
	nk_layout_row_push(ctx, bounds.w - TEXT_WIDTH - 4);
	inner_bounds = nk_widget_bounds(ctx);
	nk_combobox(ctx,
	            dev_names,
	            num_devices,
	            &_selected_idx,
	            DATA_ITEM_HEIGHT,
	            nk_vec2(inner_bounds.w, num_devices * bounds.h + 10)
	);

	if (prev_selected != _selected_idx) {
		samplerate = audio_open_device(_selected_idx);
		if (samplerate < 0) return;
#ifndef NDEBUG
		printf("Samplerate: %d\n", samplerate);
#endif
		decoder_set_samplerate(samplerate);
	}
}
