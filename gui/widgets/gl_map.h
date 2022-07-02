#ifndef gl_map_h
#define gl_map_h

#include <GLES3/gl3.h>
#include <stdlib.h>
#include "decode.h"

#define MAP_TILE_WIDTH 1024
#define MAP_TILE_HEIGHT 1024

typedef struct {
	float center_x, center_y, zoom;

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

void gl_map_init(GLMap *map);
void gl_map_vector(GLMap *map, int width, int height, const GeoPoint *data, size_t len);
void gl_map_deinit(GLMap *map);

#endif
