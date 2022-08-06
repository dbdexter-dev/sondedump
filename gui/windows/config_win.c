#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gui/widgets/gl_map_raster.h"
#include "config_win.h"
#include "gui/nuklear/nuklear_ext.h"
#include "gui/style.h"
#include "log/log.h"
#include "utils.h"

void
config_window(struct nk_context *ctx, UIState *state, Config *config, float width, float height)
{
	static Config saved_config;
	const int label_len = 120;
	const char *title = "Settings";
	const enum nk_panel_flags win_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR
	                                    | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE;
	const enum nk_panel_flags picker_flags = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_MOVABLE
	                                       | NK_WINDOW_CLOSABLE | NK_WINDOW_TITLE;
	struct nk_rect bounds;
	struct nk_rect window_bounds;
	float border;

	/* On the first invocation, populate fields with data from the configuration */
	if (state->config_open < 2) {
		sprintf(state->lat, "%.6f", config->receiver.lat);
		sprintf(state->lon, "%.6f", config->receiver.lon);
		sprintf(state->alt, "%.0f", config->receiver.alt);
		strncpy(state->tile_endpoint, config->tile_base_url, sizeof(state->tile_endpoint));

		memcpy(&saved_config, config, sizeof(saved_config));

		state->config_open = 2;
	}

	window_bounds.x = (width - PANEL_WIDTH) / 2;
	window_bounds.y = height / 2;
	window_bounds.w = PANEL_WIDTH * config->ui_scale;
	window_bounds.h = 0;

	if (nk_begin(ctx, title, window_bounds, win_flags)) {
		border = nk_window_get_panel(ctx)->border;

		/* Various purely display-related options */
		if (nk_tree_push(ctx, NK_TREE_TAB, "Display options", NK_MAXIMIZED)) {
			nk_layout_row_begin(ctx, NK_STATIC, STYLE_DEFAULT_ROW_HEIGHT * 1.5 * config->ui_scale, 1);

			/* UI scale */
			bounds = nk_layout_widget_bounds(ctx);
			nk_layout_row_push(ctx, bounds.w);
			nk_property_float(ctx, "#UI scale", 0.5, &config->ui_scale, 5, 0.1, 0.1);

			nk_layout_row_end(ctx);
			nk_layout_row_begin(ctx, NK_STATIC, STYLE_DEFAULT_ROW_HEIGHT * 1.5 * config->ui_scale, 2);

			/* Tile API base URL */
			nk_layout_row_push(ctx, label_len / 2 * config->ui_scale);
			nk_label(ctx, "Tile URL", NK_TEXT_LEFT);
			bounds = nk_layout_widget_bounds(ctx);
			nk_layout_row_push(ctx, bounds.w - label_len / 2 * config->ui_scale - 2 * border);
			nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, state->tile_endpoint, LEN(state->tile_endpoint), 0);

			nk_layout_row_end(ctx);

			/* Clear cache button */
			nk_layout_row_dynamic(ctx, STYLE_DEFAULT_BUTTON_HEIGHT * config->ui_scale, 1);
			if (nk_button_label(ctx, "Clear tile cache")) {
				gl_raster_map_flush(&state->raster_map);
			}

			nk_tree_pop(ctx);
		}

		if (nk_tree_push(ctx, NK_TREE_TAB, "Colors", NK_MAXIMIZED)) {
			nk_layout_row_begin(ctx, NK_STATIC, STYLE_DEFAULT_ROW_HEIGHT * config->ui_scale, 2);

			bounds = nk_layout_widget_bounds(ctx);

			nk_layout_row_push(ctx, bounds.w - 2 * border - STYLE_DEFAULT_ROW_HEIGHT);
			nk_label(ctx, "Temperature", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, STYLE_DEFAULT_ROW_HEIGHT);
			if (nk_button_colorf(ctx, *(struct nk_colorf*)config->colors.temp)) {
				state->picked_color = (struct nk_colorf*)config->colors.temp;
			}

			nk_layout_row_push(ctx, bounds.w - 2 * border - STYLE_DEFAULT_ROW_HEIGHT);
			nk_label(ctx, "Dew point", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, STYLE_DEFAULT_ROW_HEIGHT);
			if (nk_button_colorf(ctx, *(struct nk_colorf*)config->colors.dewpt)) {
				state->picked_color = (struct nk_colorf*)config->colors.dewpt;
			}

			nk_layout_row_push(ctx, bounds.w - 2 * border - STYLE_DEFAULT_ROW_HEIGHT);
			nk_label(ctx, "Humidity", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, STYLE_DEFAULT_ROW_HEIGHT);
			if (nk_button_colorf(ctx, *(struct nk_colorf*)config->colors.rh)) {
				state->picked_color = (struct nk_colorf*)config->colors.rh;
			}

			nk_layout_row_push(ctx, bounds.w - 2 * border - STYLE_DEFAULT_ROW_HEIGHT);
			nk_label(ctx, "Pressure", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, STYLE_DEFAULT_ROW_HEIGHT);
			if (nk_button_colorf(ctx, *(struct nk_colorf*)config->colors.press)) {
				state->picked_color = (struct nk_colorf*)config->colors.press;
			}

			nk_layout_row_push(ctx, bounds.w - 2 * border - STYLE_DEFAULT_ROW_HEIGHT);
			nk_label(ctx, "Altitude", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, STYLE_DEFAULT_ROW_HEIGHT);
			if (nk_button_colorf(ctx, *(struct nk_colorf*)config->colors.alt)) {
				state->picked_color = (struct nk_colorf*)config->colors.alt;
			}

			nk_layout_row_push(ctx, bounds.w - 2 * border - STYLE_DEFAULT_ROW_HEIGHT);
			nk_label(ctx, "Speed", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, STYLE_DEFAULT_ROW_HEIGHT);
			if (nk_button_colorf(ctx, *(struct nk_colorf*)config->colors.spd)) {
				state->picked_color = (struct nk_colorf*)config->colors.spd;
			}

			nk_layout_row_push(ctx, bounds.w - 2 * border - STYLE_DEFAULT_ROW_HEIGHT);
			nk_label(ctx, "Heading", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, STYLE_DEFAULT_ROW_HEIGHT);
			if (nk_button_colorf(ctx, *(struct nk_colorf*)config->colors.hdg)) {
				state->picked_color = (struct nk_colorf*)config->colors.hdg;
			}

			nk_layout_row_push(ctx, bounds.w - 2 * border - STYLE_DEFAULT_ROW_HEIGHT);
			nk_label(ctx, "Climb", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, STYLE_DEFAULT_ROW_HEIGHT);
			if (nk_button_colorf(ctx, *(struct nk_colorf*)config->colors.climb)) {
				state->picked_color = (struct nk_colorf*)config->colors.climb;
			}

			nk_layout_row_end(ctx);
			nk_tree_pop(ctx);
		}

		/* Ground station location configuration */
		if (nk_tree_push(ctx, NK_TREE_TAB, "Ground station location", NK_MAXIMIZED)) {
			nk_layout_row_begin(ctx, NK_STATIC, STYLE_DEFAULT_ROW_HEIGHT * config->ui_scale, 2);

			/* Latitude text box */
			nk_layout_row_push(ctx, label_len * config->ui_scale);
			nk_label(ctx, "Latitude ('N):", NK_TEXT_LEFT);
			bounds = nk_layout_widget_bounds(ctx);
			nk_layout_row_push(ctx, bounds.w - label_len * config->ui_scale - 2 * border);
			nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, state->lat, LEN(state->lat), nk_filter_float);

			/* Longitude text box */
			nk_layout_row_push(ctx, label_len * config->ui_scale);
			nk_label(ctx, "Longitude ('E):", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, bounds.w - label_len * config->ui_scale - 2 * border);
			nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, state->lon, LEN(state->lon), nk_filter_float);

			/* Altitude text box */
			nk_layout_row_push(ctx, label_len * config->ui_scale);
			nk_label(ctx, "Altitude (m):", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, bounds.w - label_len * config->ui_scale - 2 * border);
			nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, state->alt, LEN(state->alt), nk_filter_float);

			nk_layout_row_end(ctx);

			nk_tree_pop(ctx);
		}


		/* Save/cancel */
		nk_layout_row_dynamic(ctx, STYLE_DEFAULT_ROW_HEIGHT * config->ui_scale, 2);
		if (nk_button_label(ctx, "Cancel")) {
			state->config_open = 0;
			state->picked_color = NULL;

			log_debug("Reverting config");

			memcpy(config, &saved_config, sizeof(saved_config));
		}
		if (nk_button_label(ctx, "Save")) {
			state->config_open = 0;
			state->picked_color = NULL;

			log_debug("Copying config to main struct");

			/* Save config to struct */
			config->receiver.lat = atof(state->lat);
			config->receiver.lon = atof(state->lon);
			config->receiver.alt = atof(state->alt);
			strncpy(config->tile_base_url, state->tile_endpoint, sizeof(config->tile_base_url));
		}


		nk_window_fit_to_content(ctx);
		state->over_window |= nk_window_is_hovered(ctx);
	} else {
		state->config_open = 0;
		state->picked_color = NULL;

		log_debug("Reverting config");

		memcpy(config, &saved_config, sizeof(saved_config));
	}
	nk_end(ctx);

	/* If the user has picked a color to change, also draw a color picker window
	 * at the current mouse coordinates */
	window_bounds.w = PICKER_SIZE * config->ui_scale;
	window_bounds.h = 0;
	window_bounds.x = state->mouse.x - window_bounds.w;
	window_bounds.y = state->mouse.y;

	if (state->picked_color) {
		if (nk_begin(ctx, "Color picker", window_bounds, picker_flags)) {
			border = nk_window_get_panel(ctx)->border;

			nk_layout_row_dynamic(ctx, window_bounds.w - 2 * border, 1);
			nk_color_pick(ctx, state->picked_color, NK_RGB);

			state->over_window |= nk_window_is_hovered(ctx);
			nk_window_fit_to_content(ctx);
		} else {
			state->picked_color = NULL;
		}
		nk_end(ctx);
	}
}
