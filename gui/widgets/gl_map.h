#ifndef gl_map_h
#define gl_map_h

#include <stdlib.h>
#include "libs/glad/glad.h"
#include "config.h"
#include "decode.h"
#include "gui/backend/tilecache.h"

#define MAP_TILE_WIDTH 1024
#define MAP_TILE_HEIGHT 1024


typedef struct {
	int x, y, z;
	int in_use;
} Texture;

typedef struct {
	Texture *textures;
	int texture_count;
	struct {

		int x_start, x_count;
		int y_start, y_count;
		int zoom;

		int vertex_count;
	} vram_tile_metadata;

	TileCache cache;

	GLuint vao, vbo, ibo;

	GLuint tile_program;
	GLuint texture_program;

	GLuint u4f_map_color, u4m_proj;
	GLuint u1i_texture;
} GLMap;

/**
 * Initialize a OpenGL-based world map plot
 *
 * @param ctx map context, will be configured by the init function
 */
void gl_map_init(GLMap *ctx);

/**
 * Deinitialize a OpenGL-based world map plot
 *
 * @param ctx map context to deinitialize
 */
void gl_map_deinit(GLMap *ctx);

/**
 * Render a vector world map
 *
 * @param ctx       map context
 * @param config    global config
 * @param width     viewport width, in pixels
 * @param height    viewport height, in pixels
 */
void gl_map_vector(GLMap *ctx, const Config *conf, int width, int height);

#endif
