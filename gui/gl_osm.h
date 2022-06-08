#ifndef gl_osm_h
#define gl_osm_h

#include <GL/gl.h>

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

	GLuint gl_program;
	GLuint vert_shader, frag_shader;
	GLuint u1i_texture, u4m_proj, u4m_camera;
} GLOpenStreetMap;

void gl_openstreetmap_init(GLOpenStreetMap *map);
void gl_openstreetmap_raster(GLOpenStreetMap *map, int width, int height, float lat, float lon, float zoom);
void gl_openstreetmap_deinit(GLOpenStreetMap *map);

#endif
