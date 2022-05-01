#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <pthread.h>
#include "decode.h"
#include "gui.h"
#include "utils.h"
#include "nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gl3.h"
#include "style.h"
#include "widgets/data.h"

#define MAX_VERTEX_MEMORY (512 * 1024)
#define MAX_ELEMENT_MEMORY (128 * 1024)

static void *gui_main(void *args);

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
	const enum nk_panel_flags win_flags = NK_WINDOW_NO_SCROLLBAR;
	SDL_Window *win;
	SDL_GLContext glContext;
	SDL_Event evt;
	int width, height;
	int last_slot = get_slot();
	char title[64];

	/* Initialize SDL2 */
	SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	win = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                       WINDOW_WIDTH, WINDOW_HEIGHT,
	                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

	glContext = SDL_GL_CreateContext(win);
	SDL_GetWindowSize(win, &width, &height);

	/* Initialize GLEW */
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glewExperimental = 1;
	if (glewInit() != GLEW_OK) return NULL;

	/* Initialize nuklear */
	ctx = nk_sdl_init(win);
	gui_load_fonts(ctx);
	gui_set_style_default(ctx);

	while (!_interrupted) {
		/* Handle inputs */
		nk_input_begin(ctx);
		while (SDL_PollEvent(&evt)) {
			switch (evt.type) {
				case SDL_QUIT:
					goto cleanup;
					break;
				default:
					break;
			}
			nk_sdl_handle_event(&evt);
		}
		nk_input_end(ctx);

		if (last_slot != get_slot()) {
			last_slot = get_slot();
			sprintf(title, WINDOW_TITLE " - %s", get_data()->serial);
			SDL_SetWindowTitle(win, title);
		}

		/* Compose GUI */
		if (nk_begin(ctx, WINDOW_TITLE, nk_rect(0, 0, width, height), win_flags)) {
			/* Raw data */
			widget_data(ctx, width, height);

			nk_end(ctx);
		}

		/* Draw GUI */
		SDL_GetWindowSize(win, &width, &height);
		glViewport(0, 0, width, height);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0, 0, 0, 1);
		nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
		SDL_GL_SwapWindow(win);
	}

cleanup:
	_interrupted = 1;
	nk_sdl_shutdown();
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return NULL;
}
