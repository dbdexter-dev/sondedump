#include <assert.h>
#include <curl/curl.h>
#include <SDL2/SDL.h>
#include "libs/glad/glad.h"
#include "config.h"
#include "decode.h"
#include "gui.h"
#include "log/log.h"
#include "utils.h"
#include "nuklear/nuklear.h"
#include "nuklear/nuklear_ext.h"
#include "nuklear/nuklear_sdl_gl3.h"
#include "compat/threads.h"
#include "style.h"
#include "widgets/data.h"
#include "widgets/export.h"
#include "widgets/type_select.h"
#include "windows/config_win.h"
#include "windows/overview_win.h"
#include "windows/export_win.h"

#define MAX_VERTEX_MEMORY (512 * 1024)
#define MAX_ELEMENT_MEMORY (128 * 1024)

static thread_ret_t gui_main(void *args);
static thread_t _tid;
extern volatile int _interrupted;

void
gui_init(void)
{
	long curl_flags = CURL_GLOBAL_SSL;

#ifdef _WIN32
	curl_flags |= CURL_GLOBAL_WIN32;
#endif


	curl_global_init(curl_flags);
	_interrupted = 0;
	_tid = thread_create(gui_main, NULL);
}

void
gui_deinit(void)
{
	_interrupted = 1;
	if (_tid) {
		thread_join(_tid);
	}
}

static thread_ret_t
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

	UIState ui_state;
	Config config;
	const char *gl_version;


	/* Initialize SDL2 */
	SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		log_error("SDL failed to initialize: %s", SDL_GetError());
		return NULL;
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
		return NULL;
	}

	/* Create context and init GLAD */
	glContext = SDL_GL_CreateContext(win);
	if (!glContext) {
		log_error("SDL context creation failed: %s", SDL_GetError());
		return NULL;
	}

	gladLoadGLES2Loader(SDL_GL_GetProcAddress);

	/* Check results */
	gl_version = (const char*)glGetString(GL_VERSION);
	if (gl_version) {
		log_info("Created OpenGL context: %s", gl_version);
	} else {
		log_error("Unable to create OpenGL context: %s (OpenGL status %d)", SDL_GetError(), glGetError());
		return NULL;
	}

	/* Get initial window width/height */
	SDL_GetWindowSize(win, &width, &height);

	/* Load config */
	config_load_from_file(&config);

	/* Initialize map */
	//gl_map_init(&map);
	gl_raster_map_init(&ui_state.raster_map);
	gl_ground_track_init(&ui_state.ground_track, 256, 256);

	/* Initialize skew-t plot */
	gl_skewt_init(&ui_state.skewt);
	ui_state.skewt.center_x = 0.5;
	ui_state.skewt.center_y = 0.5;
	ui_state.skewt.zoom = MIN(width, height);

	/* Initialize timeseries plot */
	gl_timeseries_init(&ui_state.timeseries);

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
					                    / 256 / powf(2, config.map.zoom);
					config.map.center_y += (ui_state.mouse.y - height/2.0)
					                    / 256 / powf(2, config.map.zoom);

					config.map.zoom += 0.1 * evt.wheel.y;
					log_debug("Zoom changed: %f", config.map.zoom);

					config.map.center_x -= (ui_state.mouse.x - width/2.0)
					                    / 256 / powf(2, config.map.zoom);
					config.map.center_y -= (ui_state.mouse.y - height/2.0)
					                    / 256 / powf(2, config.map.zoom);
					break;
				case GUI_SKEW_T:
					ui_state.skewt.center_x += (ui_state.mouse.x - width/2.0) / ui_state.skewt.zoom;
					ui_state.skewt.center_y += (ui_state.mouse.y - height/2.0) / ui_state.skewt.zoom;
					ui_state.skewt.zoom *= (1 + evt.wheel.y / 10.0f);
					ui_state.skewt.center_x -= (ui_state.mouse.x - width/2.0) / ui_state.skewt.zoom;
					ui_state.skewt.center_y -= (ui_state.mouse.y - height/2.0) / ui_state.skewt.zoom;
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
						config.map.center_x -= (float)evt.motion.xrel / 256 / powf(2, config.map.zoom);
						config.map.center_y -= (float)evt.motion.yrel / 256 / powf(2, config.map.zoom);
						break;
					case GUI_SKEW_T:
						ui_state.skewt.center_x -= (float)evt.motion.xrel / ui_state.skewt.zoom;
						ui_state.skewt.center_y -= (float)evt.motion.yrel / ui_state.skewt.zoom;
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
			//gl_map_vector(&map, &config, width, height);
			gl_map_raster(&ui_state.raster_map, &config, width, height);
			gl_ground_track_data(&ui_state.ground_track, &config, width, height, get_track_data(), get_data_count());
			break;
		case GUI_TIMESERIES_PTU:
			gl_timeseries_ptu(&ui_state.timeseries, &config, get_track_data(), get_data_count(), get_data_maxima(), get_data_minima());
			break;
		case GUI_TIMESERIES_GPS:
			gl_timeseries_gps(&ui_state.timeseries, &config, get_track_data(), get_data_count(), get_data_maxima(), get_data_minima());
			break;
		case GUI_SKEW_T:
			gl_skewt_vector(&ui_state.skewt, &config, width, height, get_track_data(), get_data_count());
			break;
		}

		/* Draw nuklear GUI elements */
		nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);

		/* Swap buffers */
		SDL_GL_SwapWindow(win);
	}

cleanup:
	_interrupted = 1;
	//gl_map_deinit(&map);
	gl_raster_map_deinit(&ui_state.raster_map);
	gl_skewt_deinit(&ui_state.skewt);
	gl_timeseries_deinit(&ui_state.timeseries);
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
