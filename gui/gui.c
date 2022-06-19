#include <assert.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <GLES3/gl3.h>
#include <pthread.h>
#include "decode.h"
#include "gui.h"
#include "utils.h"
#include "nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gl3.h"
#include "style.h"
#include "widgets/audio_dev_select.h"
#include "widgets/chart.h"
#include "widgets/data.h"
#include "widgets/type_select.h"
#include "gl_osm.h"

#define MAX_VERTEX_MEMORY (512 * 1024)
#define MAX_ELEMENT_MEMORY (128 * 1024)

static void *gui_main(void *args);
static void overview_window(struct nk_context *ctx, float *center_x, float *center_y);

static pthread_t _tid;
static enum graph active_visualization;
extern volatile int _interrupted;

void
gui_init(void)
{
	_interrupted = 0;
	active_visualization = GUI_TIMESERIES;
	pthread_create(&_tid, NULL, gui_main, NULL);
}

void
gui_deinit(void)
{
	void *retval;

	_interrupted = 1;
	if (_tid) pthread_join(_tid, &retval);
}

enum graph
gui_get_graph(void)
{
	return active_visualization;
}

void
gui_set_graph(enum graph visual)
{
	active_visualization = visual;
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
	float center_x, center_y, zoom;
	int last_slot = get_slot();
	char title[64];
	GLOpenStreetMap map;
	int pollcount, dragging;
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
	if (SDL_GetError()[0] != 0) {
		printf("SDL context creation failed: %s\n", SDL_GetError());
	}
#endif
	SDL_GetWindowSize(win, &width, &height);

	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

	/* Initialize map */
	gl_openstreetmap_init(&map);
	zoom = 7.0;
	center_x = lon_to_x(9.33, 0);
	center_y = lat_to_y(45.5, 0);
	dragging = 0;
#ifndef NDEBUG
	printf("Starting xy: %f %f\n", center_x, center_y);
#endif

	/* Initialize nuklear */
	ctx = nk_sdl_init(win);
	gui_load_fonts(ctx);
	gui_set_style_default(ctx);

	/* Force refresh */
	pollcount = 1;

	while (!_interrupted) {
		/* When requested, bypass waitevent and go straight to event processing */
		if (pollcount) {
			pollcount--;
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
				zoom += 0.1 * evt.wheel.y;
				break;
			case SDL_MOUSEBUTTONDOWN:
				dragging = 1;
				break;
			case SDL_MOUSEBUTTONUP:
				dragging = 0;
				break;
			case SDL_MOUSEMOTION:
				if (dragging) {
					center_x -= (float)evt.motion.xrel / MAP_TILE_WIDTH / powf(2, zoom);
					center_y -= (float)evt.motion.yrel / MAP_TILE_HEIGHT / powf(2, zoom);
				}
				break;
			case SDL_USEREVENT:
				break;
			default:
				/* Force refresh  */
				pollcount = 1;
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
		SDL_GetWindowSize(win, &width, &height);

		/* Compose GUI */
		overview_window(ctx, &center_x, &center_y);

		/* Render */
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0, 0, 0, 1);

		/* Draw background */
		gl_openstreetmap_vector(&map, width, height, center_x, center_y, zoom);

		/* Draw GUI */
		nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);

		SDL_GL_SwapWindow(win);
	}

cleanup:
	_interrupted = 1;
	gl_openstreetmap_deinit(&map);
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
overview_window(struct nk_context *ctx, float *center_x, float *center_y)
{
	const GeoPoint *last_point;
	const enum nk_panel_flags win_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR
	                                    | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE;

	/* Compose GUI */
	if (nk_begin(ctx, "Overview", nk_rect(0, 0, 400, 520), win_flags)) {
		/* Audio device selection TODO handle input from file */
		widget_audio_dev_select(ctx);

		/* Sonde type selection */
		widget_type_select(ctx);

		nk_layout_row_dynamic(ctx, 400, 1);
		if (nk_group_begin(ctx, "Data", NK_WINDOW_NO_SCROLLBAR)) {
			/* Raw data */
			widget_data(ctx);

			nk_group_end(ctx);
		}

		nk_layout_row_dynamic(ctx, 25, 1);
		if (nk_button_label(ctx, "Re-center map")) {
			if (get_data_count() > 0) {
				last_point = get_track_data() + get_data_count() - 1;
				*center_x = lon_to_x(last_point->lon, 0);
				*center_y = lat_to_y(last_point->lat, 0);
			}
		}


	}
	nk_end(ctx);
}
/* }}} */
