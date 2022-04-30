#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <pthread.h>
#include "gui.h"
#include "utils.h"
#include "nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gl3.h"

#define MAX_VERTEX_MEMORY (512 * 1024)
#define MAX_ELEMENT_MEMORY (128 * 1024)

static void *gui_main(void *args);

static pthread_t _tid;
static int _running;

void
gui_init(void)
{
	_running = 1;
	pthread_create(&_tid, NULL, gui_main, NULL);
}

void
gui_deinit(void)
{
	void *retval;

	_running = 0;
	pthread_join(_tid, &retval);
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

	/* Initialize SDL2 */
	SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	win = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                       WINDOW_WIDTH, WINDOW_HEIGHT,
	                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);

	glContext = SDL_GL_CreateContext(win);
	SDL_GetWindowSize(win, &width, &height);

	/* Initialize GLEW */
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glewExperimental = 1;
	if (glewInit() != GLEW_OK) return NULL;

	/* Initialize nuklear */
	ctx = nk_sdl_init(win);

	/* Load fonts */
	{
		struct nk_font_atlas *atlas;
		nk_sdl_font_stash_begin(&atlas);
		nk_sdl_font_stash_end();
	}


	while (_running) {
		/* Handle inputs */
		nk_input_begin(ctx);
		while (SDL_PollEvent(&evt)) {
			switch (evt.type) {
				case SDL_QUIT:
					_running = 0;
					goto cleanup;
					break;
				default:
					break;
			}
		}
		nk_input_end(ctx);

		/* Compose GUI */
		if (nk_begin(ctx, WINDOW_TITLE, nk_rect(0, 0, width, height), 0)) {
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
	nk_sdl_shutdown();
	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return NULL;
}
