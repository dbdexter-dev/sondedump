#include "decode.h"
#include "gui/nuklear/nuklear_ext.h"
#include "gui/style.h"
#include "gui/widgets/audio_dev_select.h"
#include "gui/widgets/type_select.h"
#include "gui/widgets/data.h"
#include "log/log.h"
#include "overview_win.h"
#include "utils.h"

void
overview_window(struct nk_context *ctx, UIState *state, Config *config)
{
	float size;
	struct nk_vec2 position;
	const char *title = "Overview";
	const GeoPoint *last_point;
	const enum nk_panel_flags win_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR
	                                    | NK_WINDOW_MINIMIZABLE | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE;


	/* Compose GUI */
	if (nk_begin(ctx, title, nk_rect(0, 0, PANEL_WIDTH * config->ui_scale, 0), win_flags)) {
		/* Audio device selection TODO handle input from file */
		widget_audio_dev_select(ctx, config->ui_scale);

		/* Sonde type selection */
		widget_type_select(ctx, config->ui_scale);

		/* UI graph selection */
		nk_layout_row_dynamic(ctx, STYLE_DEFAULT_ROW_HEIGHT * config->ui_scale, 4);
		if (nk_option_label(ctx, "Map", state->active_widget == GUI_MAP)) {
			state->active_widget = GUI_MAP;
		}
		if (nk_option_label(ctx, "PTU data", state->active_widget == GUI_TIMESERIES_PTU)) {
			state->active_widget = GUI_TIMESERIES_PTU;
		}
		if (nk_option_label(ctx, "GPS data", state->active_widget == GUI_TIMESERIES_GPS)) {
			state->active_widget = GUI_TIMESERIES_GPS;
		}
		if (nk_option_label(ctx, "Skew-T", state->active_widget == GUI_SKEW_T)) {
			state->active_widget = GUI_SKEW_T;
		}

		/* Raw live data */
		size = widget_data_base_size(ctx, config->receiver.lat, config->receiver.lon, config->receiver.alt);
		nk_layout_row_dynamic(ctx, size * config->ui_scale, 1);
		if (nk_group_begin(ctx, "Data", NK_WINDOW_NO_SCROLLBAR)) {
			widget_data(ctx, config->receiver.lat, config->receiver.lon, config->receiver.alt, config->ui_scale);

			nk_group_end(ctx);
		}

		nk_layout_row_dynamic(ctx, STYLE_DEFAULT_BUTTON_HEIGHT * config->ui_scale, 1);
		/* Export data */
		if (nk_button_label(ctx, "Export...")) {
			state->export_open = 1;
			log_debug("Opening export window");
		}

		/* Open config window, pre-populating fields with the current config */
		if (nk_button_label(ctx, "Configure...")) {
			state->config_open = 1;
			log_debug("Opening config window");
		}

		if (state->active_widget == GUI_MAP) {
			/* Reset map view */
			if (nk_button_label(ctx, "Re-center")) {
				if (get_data_count() > 0) {
					last_point = get_track_data() + get_data_count() - 1;
					config->map.center_x = lon_to_x(last_point->lon, 0);
					config->map.center_y = lat_to_y(last_point->lat, 0);
				}
			}

			/* FIXME this changes the layout for future widgets, though there is
			 * nothing there for now */
			nk_layout_row_dynamic(ctx, 1.8 * STYLE_DEFAULT_ROW_HEIGHT * config->ui_scale, 1);
			nk_label_wrap(ctx, "Map data from OpenStreetMap: https://openstreetmap.org/copyright");
		}


		/* Resize according to the scale factor */
		ctx->current->bounds.w = PANEL_WIDTH * config->ui_scale;
		position = nk_window_get_position(ctx);

		/* Resize to fit contents in the y direction */
		nk_window_fit_to_content(ctx);

		/* Bring window back in bounds */
		position.x = MAX(0, position.x);
		position.y = MAX(0, position.y);
		nk_window_set_position(ctx, title, position);

		state->over_window |= nk_window_is_hovered(ctx);
	}

	nk_end(ctx);
}
