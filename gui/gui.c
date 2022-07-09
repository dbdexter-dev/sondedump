#include <assert.h>
#include <SDL2/SDL.h>
#include "libs/glad/glad.h"
#ifdef _MSC_VER
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "config.h"
#include "decode.h"
#include "gui.h"
#include "log/log.h"
#include "utils.h"
#include "nuklear/nuklear.h"
#include "nuklear/nuklear_ext.h"
#include "nuklear/nuklear_sdl_gl3.h"
#include "style.h"
#include "widgets/audio_dev_select.h"
#include "widgets/data.h"
#include "widgets/export.h"
#include "widgets/type_select.h"
#include "widgets/gl_map.h"
#include "widgets/gl_skewt.h"
#include "widgets/gl_timeseries.h"

#define MAX_VERTEX_MEMORY (512 * 1024)
#define MAX_ELEMENT_MEMORY (128 * 1024)

typedef struct {
	struct {
		float x, y;
	} mouse;
	float old_scale;
	struct nk_colorf *picked_color;

	int config_open, export_open;
	int over_window;
	int dragging;
	int force_refresh;

	char lat[32], lon[32], alt[32];

	enum graph active_widget;
} UIState;

static void *gui_main(void *args);
static void config_window(struct nk_context *ctx, UIState *state, Config *conf, float width, float height);
static void overview_window(struct nk_context *ctx, UIState *state, Config *conf);
static void export_window(struct nk_context *ctx, UIState *state, Config *conf, float width, float height);

#ifdef _MSC_VER
typedef HANDLE pthread_t;
#endif

static pthread_t _tid;
extern volatile int _interrupted;

void
gui_init(void)
{
	_interrupted = 0;
#ifdef _MSC_VER
	_tid = CreateThread(NULL, 0, gui_main, NULL, 0, NULL);
#else
	pthread_create(&_tid, NULL, gui_main, NULL);
#endif
}

void
gui_deinit(void)
{
	void *retval;

	_interrupted = 1;
	if (_tid) {
#ifdef _MSC_VER
		WaitForSingleObject(_tid, INFINITE);
#else
		pthread_join(_tid, &retval);
#endif
	}
}

static void*
gui_main(void *args)
{
	(void)args;

	struct nk_context *ctx;
	SDL_Window *win;
	SDL_GLContext glContext;
	SDL_Event evt;
	int width, height;
	int last_slot = get_slot();
	int gl_error;
	char title[64];

	GLMap map;
	GLSkewT skewt;
	GLTimeseries timeseries;
	UIState ui_state;
	Config config;
	const char *gl_version;


	/* Initialize SDL2 */
	SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		log_error("SDL failed to initialize: %s", SDL_GetError());
		return (void*)1;
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	win = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                       WINDOW_WIDTH, WINDOW_HEIGHT,
	                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
	if (!win) {
		log_error("SDL window creation failed: %s", SDL_GetError());
		return (void*)1;
	}

	/* Create context and init GLAD */
	glContext = SDL_GL_CreateContext(win);
	if (!glContext) {
		log_error("SDL context creation failed: %s", SDL_GetError());
		return (void*)1;
	}

	gladLoadGLES2Loader(SDL_GL_GetProcAddress);

	/* Check results */
	gl_version = (const char*)glGetString(GL_VERSION);
	if (gl_version) {
		log_debug("Created OpenGL context: %s", gl_version);
	} else {
		log_error("Unable to create OpenGL context: %s (OpenGL status %d)", SDL_GetError(), glGetError());
		return (void*)1;
	}

	/* Get initial window width/height */
	SDL_GetWindowSize(win, &width, &height);

	/* Load config */
	config_load_from_file(&config);

	/* Initialize map */
	gl_map_init(&map);

	/* Initialize skew-t plot */
	gl_skewt_init(&skewt);
	skewt.center_x = 0.5;
	skewt.center_y = 0.5;
	skewt.zoom = MIN(width, height);

	/* Initialize timeseries plot */
	gl_timeseries_init(&timeseries);

	/* Initialize nuklear */
	ctx = nk_sdl_init(win);
	gui_load_fonts(ctx, config.ui_scale);
	gui_set_style_default(ctx);

	/* Initialize gui state */
	ui_state.old_scale = config.ui_scale;
	ui_state.picked_color = NULL;
	ui_state.force_refresh = 1;
	ui_state.dragging = 0;
	ui_state.over_window = 0;
	ui_state.config_open = 0;
	ui_state.export_open = 0;
	ui_state.active_widget = GUI_MAP;

	while (!_interrupted) {
		/* Just make sure that everything is still fine */
		gl_error = glGetError();
		if (gl_error) log_warn("Detected OpenGL non-zero status: %d", gl_error);

		/* When requested, bypass waitevent and go straight to event processing */
		if (ui_state.force_refresh) {
			ui_state.force_refresh--;
			evt.type = SDL_USEREVENT;
		} else {
			SDL_WaitEvent(&evt);
		}

		/* Handle inputs */
		nk_input_begin(ctx);
		do {
			switch (evt.type) {
			case SDL_QUIT:
				goto cleanup;
			case SDL_MOUSEWHEEL:
				/* Zoom to where the cursor is */
				switch (ui_state.active_widget) {
				case GUI_MAP:
					config.map.center_x += (ui_state.mouse.x - width/2.0)
					                    / MAP_TILE_WIDTH / powf(2, config.map.zoom);
					config.map.center_y += (ui_state.mouse.y - height/2.0)
					                    / MAP_TILE_HEIGHT / powf(2, config.map.zoom);

					config.map.zoom += 0.1 * evt.wheel.y;

					config.map.center_x -= (ui_state.mouse.x - width/2.0)
					                    / MAP_TILE_WIDTH / powf(2, config.map.zoom);
					config.map.center_y -= (ui_state.mouse.y - height/2.0)
					                    / MAP_TILE_HEIGHT / powf(2, config.map.zoom);
					break;
				case GUI_SKEW_T:
					skewt.center_x += (ui_state.mouse.x - width/2.0) / skewt.zoom;
					skewt.center_y += (ui_state.mouse.y - height/2.0) / skewt.zoom;
					skewt.zoom *= (1 + evt.wheel.y / 10.0f);
					skewt.center_x -= (ui_state.mouse.x - width/2.0) / skewt.zoom;
					skewt.center_y -= (ui_state.mouse.y - height/2.0) / skewt.zoom;
					break;
				default:
					break;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				ui_state.dragging = !ui_state.over_window;
				break;
			case SDL_MOUSEBUTTONUP:
				ui_state.dragging = 0;
				break;
			case SDL_MOUSEMOTION:
				if (ui_state.dragging) {
					/* Drag 1:1 with cursor movement */
					switch (ui_state.active_widget) {
					case GUI_MAP:
						config.map.center_x -= (float)evt.motion.xrel / MAP_TILE_WIDTH / powf(2, config.map.zoom);
						config.map.center_y -= (float)evt.motion.yrel / MAP_TILE_HEIGHT / powf(2, config.map.zoom);
						break;
					case GUI_SKEW_T:
						skewt.center_x -= (float)evt.motion.xrel / skewt.zoom;
						skewt.center_y -= (float)evt.motion.yrel / skewt.zoom;
						break;
					default:
						break;
					}
				}
				/* Save current mouse position (used for zooming) */
				ui_state.mouse.x = evt.motion.x;
				ui_state.mouse.y = evt.motion.y;
				break;
			case SDL_USEREVENT:
				break;
			default:
				/* Force refresh */
				ui_state.force_refresh = 1;
				break;
			}

			nk_sdl_handle_event(&evt);
		} while (SDL_PollEvent(&evt));
		nk_input_end(ctx);

		/* Re-render fonts on ui scale change */
		if (config.ui_scale != ui_state.old_scale) {
			gui_load_fonts(ctx, config.ui_scale);
			ui_state.old_scale = config.ui_scale;
		}

		/* If the sonde type has changed, update title */
		if (last_slot != get_slot()) {
			last_slot = get_slot();
			sprintf(title, WINDOW_TITLE " - %s", get_data()->serial);
			SDL_SetWindowTitle(win, title);
		}

		/* Update window size */
		SDL_GetWindowSize(win, &width, &height);

		ui_state.over_window = 0;
		overview_window(ctx, &ui_state, &config);
		if (ui_state.export_open) export_window(ctx, &ui_state, &config, width, height);
		if (ui_state.config_open) config_window(ctx, &ui_state, &config, width, height);

		/* Render */
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0, 0, 0, 1);

		/* Draw background widget */
		switch (ui_state.active_widget) {
		case GUI_MAP:
			gl_map_vector(&map, &config, width, height, get_track_data(), get_data_count());
			break;
		case GUI_TIMESERIES_PTU:
			gl_timeseries_ptu(&timeseries, &config, get_track_data(), get_data_count());
			break;
		case GUI_TIMESERIES_GPS:
			gl_timeseries_gps(&timeseries, &config, get_track_data(), get_data_count());
			break;
		case GUI_SKEW_T:
			gl_skewt_vector(&skewt, &config, width, height, get_track_data(), get_data_count());
			break;
		}

		/* Draw nuklear GUI elements */
		nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);

		/* Swap buffers */
		SDL_GL_SwapWindow(win);
	}

cleanup:
	_interrupted = 1;
	gl_map_deinit(&map);
	gl_skewt_deinit(&skewt);
	gl_timeseries_deinit(&timeseries);
	nk_sdl_shutdown();
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(win);
	SDL_Quit();

	/* Save config */
	config_save_to_file(&config);
	return NULL;
}

void
gui_force_update(void)
{
	SDL_Event empty = {.type = SDL_USEREVENT};
	SDL_PushEvent(&empty);
}

/* Static functions {{{ */
static void
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

		/* Reset map view */
		nk_layout_row_dynamic(ctx, STYLE_DEFAULT_ROW_HEIGHT * 1.2 * config->ui_scale, 1);
		if (state->active_widget == GUI_MAP && nk_button_label(ctx, "Re-center")) {
			if (get_data_count() > 0) {
				last_point = get_track_data() + get_data_count() - 1;
				config->map.center_x = lon_to_x(last_point->lon, 0);
				config->map.center_y = lat_to_y(last_point->lat, 0);
			}
		}

		if (nk_button_label(ctx, "Export...")) {
			state->export_open = 1;
			log_debug("Opening export window");
		}

		/* Open config window, pre-populating fields with the current config */
		if (nk_button_label(ctx, "Configure...")) {
			state->config_open = 1;
			log_debug("Opening config window");
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

static void
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
			nk_layout_row_begin(ctx, NK_STATIC, STYLE_DEFAULT_ROW_HEIGHT * config->ui_scale, 1);

			/* UI scale configuration */
			bounds = nk_layout_widget_bounds(ctx);
			nk_layout_row_push(ctx, bounds.w);
			nk_property_float(ctx, "#UI scale", 0.5, &config->ui_scale, 5, 0.1, 0.1);

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

static void
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
/* }}} */
