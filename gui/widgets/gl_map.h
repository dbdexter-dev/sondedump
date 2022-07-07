#ifndef gl_map_h
#define gl_map_h

#include <stdlib.h>
#include "libs/glad/glad.h"
#include "config.h"
#include "decode.h"

#define MAP_TILE_WIDTH 1024
#define MAP_TILE_HEIGHT 1024

typedef struct {
	struct {
		int x_start, x_count;
		int y_start, y_count;
		int zoom;

		int vertex_count;
	} vram_tile_metadata;

	GLuint vao, vbo, ibo;
	GLuint track_vao, track_vbo;

	GLuint tile_program;
	GLuint tile_vert_shader, tile_frag_shader;
	GLuint track_program;
	GLuint track_vert_shader, track_frag_shader;

	GLuint u4f_map_color, u4m_proj;
	GLuint u1f_zoom, u4f_track_color, u4m_track_proj;
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
 * Render the world map w/ ground track
 *
 * @param ctx       map context
 * @param config    global config
 * @param width     viewport width, in pixels
 * @param height    viewport height, in pixels
 * @param data      data to display
 * @param len       number of elements to display in the data array
 */
void gl_map_vector(GLMap *ctx, const Config *conf, int width, int height, const GeoPoint *data, size_t len);

#endif
