#ifndef gl_map_raster_h
#define gl_map_raster_h

#include "config.h"
#include "gui/backend/tilecache.h"
#include "libs/glad/glad.h"

#define CACHE_SIZE 256

typedef struct {
	int x, y, z;
	int in_use;
} Texture;

typedef struct {
	struct {
		GLuint tex;
		Texture *metadata;
		int count;
	} vram;

	TileCache cache;

	GLuint program;
	GLuint vao, vbo, ibo;

	GLuint u4m_proj, u1i_texture;
} GLRasterMap;


void gl_raster_map_init(GLRasterMap *ctx);
void gl_raster_map_deinit(GLRasterMap *ctx);

/**
 * Render a raster world map
 *
 * @param ctx       map context
 * @param config    global config
 * @param width     viewport width, in pixels
 * @param height    viewport height, in pixels
 */
void gl_map_raster(GLRasterMap *ctx, const Config *conf, int width, int height);

void gl_raster_map_flush(GLRasterMap *ctx);

#endif
