#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <GLES3/gl3.h>
#include <pthread.h>
#include "decode.h"
#include "gui.h"
#include "utils.h"
#include "nuklear/nuklear.h"
#include "nuklear/nuklear_ext.h"
#include "nuklear/nuklear_sdl_gl3.h"
#include "style.h"
#include "widgets/audio_dev_select.h"
#include "widgets/data.h"
#include "widgets/type_select.h"
#include "widgets/gl_map.h"
#include "widgets/gl_skewt.h"

#define MAX_VERTEX_MEMORY (512 * 1024)
#define MAX_ELEMENT_MEMORY (128 * 1024)

typedef struct {
	float scale;

	int over_window;
	int config_open;
	int dragging;
	int force_refresh;
} UIState;

static void *gui_main(void *args);
static void overview_window(struct nk_context *ctx, GLMap *map, UIState *state);
static void config_window(struct nk_context *ctx, UIState *state);

static pthread_t _tid;
extern volatile int _interrupted;

void
gui_init(void)
{
	_interrupted = 0;
	pthread_create(&_tid, NULL, gui_main, NULL);
}

void
gui_deinit(void)
{
	void *retval;

	_interrupted = 1;
	if (_tid) pthread_join(_tid, &retval);
}

static void*
gui_main(void *args)
{
	(void)args;

	float old_scale;
	struct nk_context *ctx;
	SDL_Window *win;
	SDL_GLContext glContext;
	SDL_Event evt;
	int width, height;
	int last_slot = get_slot();
	char title[64];

	GLMap map;
	GLSkewT skewt;
	enum graph active_visualization;
	UIState ui_state;
	const char *gl_version;


	/* Initialize SDL2 */
	SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	win = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                       WINDOW_WIDTH, WINDOW_HEIGHT,
	                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

	glContext = SDL_GL_CreateContext(win);
	gl_version = (const char*)glGetString(GL_VERSION);
	if (gl_version) {
		printf("Created OpenGL context: %s\n", gl_version);
	} else {
		printf("Unable to create OpenGL context: %s\n", SDL_GetError());
	}
#ifndef NDEBUG
	if (!strcmp(SDL_GetError(), "")) {
		printf("SDL context creation failed: %s\n", SDL_GetError());
	}
#endif
	SDL_GetWindowSize(win, &width, &height);
	glViewport(0, 0, width, height);

	/* Initialize map */
	gl_map_init(&map);
	map.zoom = 7.0;
	map.center_x = lon_to_x(0, 0);
	map.center_y = lat_to_y(0, 0);

	/* Initialize skew-t plot */
	gl_skewt_init(&skewt);
	skewt.center_x = 0.5;
	skewt.center_y = 0.5;
	skewt.zoom = width;

#ifndef NDEBUG
	printf("Starting xy: %f %f\n", center_x, center_y);
#endif

	/* Initialize nuklear */
	ui_state.scale = old_scale = 1;
	ctx = nk_sdl_init(win);
	gui_load_fonts(ctx, ui_state.scale);
	gui_set_style_default(ctx);

	/* Initialize imgui data */
	ui_state.force_refresh = 1;
	ui_state.dragging = 0;
	ui_state.over_window = 0;
	ui_state.config_open = 0;
	active_visualization = GUI_SKEW_T;

	while (!_interrupted) {
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
				switch (active_visualization) {
				case GUI_MAP:
					map.zoom += 0.1 * evt.wheel.y;
					break;
				case GUI_SKEW_T:
					skewt.zoom *= (1 + evt.wheel.y / 10.0f);
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
					switch (active_visualization) {
					case GUI_MAP:
						map.center_x -= (float)evt.motion.xrel / MAP_TILE_WIDTH / powf(2, map.zoom);
						map.center_y -= (float)evt.motion.yrel / MAP_TILE_HEIGHT / powf(2, map.zoom);
						break;
					case GUI_SKEW_T:
						skewt.center_x -= (float)evt.motion.xrel / skewt.zoom;
						skewt.center_y += (float)evt.motion.yrel / skewt.zoom;
						break;
					default:
						break;
					}
				}
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

		/* If the sonde type has changed, update title */
		if (last_slot != get_slot()) {
			last_slot = get_slot();
			sprintf(title, WINDOW_TITLE " - %s", get_data()->serial);
			SDL_SetWindowTitle(win, title);
		}

		/* Update window size */
		SDL_GetWindowSize(win, &width, &height);

		/* Compose nuklear GUI elements */
		ui_state.over_window = 0;

		overview_window(ctx, &map, &ui_state);
		if (ui_state.config_open) config_window(ctx, &ui_state);
		if (ui_state.scale != old_scale) {
			gui_load_fonts(ctx, ui_state.scale);
			old_scale = ui_state.scale;
		}

		/* Render */
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0, 0, 0, 1);

		/* Draw background map */
		switch (active_visualization) {
		case GUI_MAP:
			gl_map_vector(&map, width, height);
			break;
		case GUI_TIMESERIES:
			break;
		case GUI_SKEW_T:
			gl_skewt_vector(&skewt, width, height);
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
	nk_sdl_shutdown();
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(win);
	SDL_Quit();
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
overview_window(struct nk_context *ctx, GLMap *map, UIState *state)
{
	struct nk_vec2 position;
	const char *title = "Overview";
	const GeoPoint *last_point;
	const enum nk_panel_flags win_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR
	                                    | NK_WINDOW_MINIMIZABLE | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE;

	/* Compose GUI */
	if (nk_begin(ctx, title, nk_rect(0, 0, PANEL_WIDTH * state->scale, 0), win_flags)) {
		/* Audio device selection TODO handle input from file */
		widget_audio_dev_select(ctx, state->scale);

		/* Sonde type selection */
		widget_type_select(ctx, state->scale);

		nk_layout_row_dynamic(ctx, 400 * state->scale, 1);
		if (nk_group_begin(ctx, "Data", NK_WINDOW_NO_SCROLLBAR)) {
			/* Raw data */
			widget_data(ctx, state->scale);

			nk_group_end(ctx);
		}

		nk_layout_row_dynamic(ctx, STYLE_DEFAULT_ROW_HEIGHT * 1.2 * state->scale, 1);
		if (nk_button_label(ctx, "Re-center map")) {
			if (get_data_count() > 0) {
				last_point = get_track_data() + get_data_count() - 1;
				map->center_x = lon_to_x(last_point->lon, 0);
				map->center_y = lat_to_y(last_point->lat, 0);
			}
		}

		if (nk_button_label(ctx, "Configure...")) {
			state->config_open = 1;
		}
	}
	/* Resize according to the scale factor */
	ctx->current->bounds.w = PANEL_WIDTH * state->scale;
	position = nk_window_get_position(ctx);

	/* Resize to fit contents in the y direction */
	nk_window_fit_to_content(ctx);

	/* Bring window back in bounds */
	position.x = MAX(0, position.x);
	position.y = MAX(0, position.y);
	nk_window_set_position(ctx, "Overview", position);

	state->over_window |= nk_window_is_hovered(ctx);
	nk_end(ctx);
}

static void
config_window(struct nk_context *ctx, UIState *state)
{
	const int label_len = 120;
	const char *title = "Settings";
	const enum nk_panel_flags win_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR
	                                    | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE;
	struct nk_rect bounds;
	float border;
	static char lat[32], lon[32], alt[32];

	if (nk_begin(ctx, title, nk_rect(0, 0, PANEL_WIDTH * state->scale, 0), win_flags)) {
		border = nk_window_get_panel(ctx)->border;

		if (nk_tree_push(ctx, NK_TREE_TAB, "Display options", NK_MAXIMIZED)) {
			nk_layout_row_begin(ctx, NK_STATIC, STYLE_DEFAULT_ROW_HEIGHT * state->scale, 1);

			/* UI scale configuration */
			bounds = nk_layout_widget_bounds(ctx);
			nk_layout_row_push(ctx, bounds.w);
			nk_property_float(ctx, "#UI scale", 1, &state->scale, 5, 0.1, 0.1);

			nk_tree_pop(ctx);
		}

		if (nk_tree_push(ctx, NK_TREE_TAB, "Ground station location", NK_MAXIMIZED)) {
			nk_layout_row_begin(ctx, NK_STATIC, STYLE_DEFAULT_ROW_HEIGHT * state->scale, 2);

			/* Latitude text box */
			nk_layout_row_push(ctx, label_len * state->scale);
			nk_label(ctx, "Latitude ('N):", NK_TEXT_LEFT);
			bounds = nk_layout_widget_bounds(ctx);
			nk_layout_row_push(ctx, bounds.w - label_len * state->scale - 2 * border);
			nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, lat, LEN(lat), nk_filter_float);

			/* Longitude text box */
			nk_layout_row_push(ctx, label_len * state->scale);
			nk_label(ctx, "Longitude ('E):", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, bounds.w - label_len * state->scale - 2 * border);
			nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, lon, LEN(lon), nk_filter_float);

			/* Altitude text box */
			nk_layout_row_push(ctx, label_len * state->scale);
			nk_label(ctx, "Altitude (m):", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, bounds.w - label_len * state->scale - 2 * border);
			nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, alt, LEN(alt), nk_filter_float);

			nk_layout_row_end(ctx);

			nk_tree_pop(ctx);
		}
	}

	nk_layout_row_dynamic(ctx, STYLE_DEFAULT_ROW_HEIGHT * state->scale, 2);
	if (nk_button_label(ctx, "Cancel")) {
		state->config_open = 0;
		/* TODO undo settings */
	}
	if (nk_button_label(ctx, "Save")) {
		state->config_open = 0;
		/* TODO copy settings */
	}

	nk_window_fit_to_content(ctx);
	state->over_window |= nk_window_is_hovered(ctx);

	nk_end(ctx);
}
/* }}} */
