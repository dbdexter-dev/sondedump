#ifndef gl_osm_h
#define gl_osm_h

#include <GLES2/gl2.h>

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

	GLuint attrib_pos;
} GLOpenStreetMap;

void gl_openstreetmap_init(GLOpenStreetMap *map);
void gl_openstreetmap_vector(GLOpenStreetMap *map, int width, int height, float lat, float lon, float zoom);
void gl_openstreetmap_deinit(GLOpenStreetMap *map);

#endif
