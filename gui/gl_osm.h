#ifndef gl_osm_h
#define gl_osm_h

#include <GLES2/gl2.h>

#define MAP_TILE_WIDTH 1024
#define MAP_TILE_HEIGHT 1024

typedef struct {
	GLuint vbo, ibo;
	int x, y, vertex_count;
	int in_use;
} Tile;

typedef struct {
	GLuint tex_array;
	Tile *tiles;
	int tile_count;

	GLuint vao;
	GLuint track_vao, track_vbo;

	GLuint texture_program;
	GLuint tex_vert_shader, tex_frag_shader;
	GLuint track_program;
	GLuint track_vert_shader, track_frag_shader;

	GLuint u4m_proj;
	GLuint u1f_zoom, u4f_track_color, u4m_track_proj;

	GLuint attrib_pos;
} GLOpenStreetMap;

void gl_openstreetmap_init(GLOpenStreetMap *map);
void gl_openstreetmap_raster(GLOpenStreetMap *map, int width, int height, float lat, float lon, float zoom);
void gl_openstreetmap_deinit(GLOpenStreetMap *map);

#endif
