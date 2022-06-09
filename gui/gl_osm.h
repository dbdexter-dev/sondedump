#ifndef gl_osm_h
#define gl_osm_h

#include <GLES2/gl2.h>

#define MAP_TILE_WIDTH 256
#define MAP_TILE_HEIGHT 256

#define TILE_MIN_ZOOM 1

#define MAX_WIDTH_TILES 16
#define MAX_HEIGHT_TILES 16

typedef struct {
	int x, y, z;
	int in_use;
} Texture;

typedef struct {
	GLuint tex_array;
	Texture *textures;
	int texture_count;

	GLuint vao, vbo, ibo;
	GLuint track_vao, track_vbo;

	GLuint texture_program;
	GLuint tex_vert_shader, tex_frag_shader;
	GLuint track_program;
	GLuint track_vert_shader, track_frag_shader;

	GLuint u1i_texture, u4m_proj;
	GLuint u1f_zoom, u4f_track_color, u4m_track_proj;
} GLOpenStreetMap;

void gl_openstreetmap_init(GLOpenStreetMap *map);
void gl_openstreetmap_raster(GLOpenStreetMap *map, int width, int height, float lat, float lon, float zoom);
void gl_openstreetmap_deinit(GLOpenStreetMap *map);

#endif
