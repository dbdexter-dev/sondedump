#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>  /* TODO Remove */
#include <GLES3/gl3.h>
#include "decode.h"
#include "gl_osm.h"
#include "style.h"
#include "utils.h"

#define MODLT(x, y, mod) (((y) - (x) - 1 + (mod)) % (mod) < ((mod) / 2))
#define MODGE(x, y, mod) (((x) - (y) + (mod)) % (mod) < ((mod) / 2))

#ifdef __APPLE__
  #define SHADER_VERSION "#version 150\n"
#else
  #define SHADER_VERSION "#version 300 es\n"
#endif

typedef struct {
	float x, y;
} Vertex;

static void map_opengl_init(GLOpenStreetMap *map);
static void track_opengl_init(GLOpenStreetMap *map);
static int mipmap(float zoom);

static void update_buffers(GLOpenStreetMap *map, int x_start, int y_start, int x_count, int y_count, int zoom);


void
gl_openstreetmap_init(GLOpenStreetMap *map)
{
	/* Background map program, buffers, uniforms... */
	map_opengl_init(map);

	/* Ground track program, buffers, uniforms... */
	track_opengl_init(map);

	map->tiles = NULL;
	map->tex_array = 0;
	map->tile_count = 0;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void
gl_openstreetmap_deinit(GLOpenStreetMap *map)
{
	int i;

	if (map->vao) glDeleteVertexArrays(1, &map->vao);
	if (map->tile_count) {
		for (i=0; i<map->tile_count; i++) {
			glDeleteBuffers(1, &map->tiles[i].vbo);
			glDeleteBuffers(1, &map->tiles[i].ibo);
		}
	}

	if (map->tex_vert_shader) glDeleteProgram(map->tex_vert_shader);
	if (map->tex_frag_shader) glDeleteProgram(map->tex_frag_shader);
	if (map->texture_program) glDeleteProgram(map->texture_program);
	if (map->track_vert_shader) glDeleteProgram(map->track_vert_shader);
	if (map->track_frag_shader) glDeleteProgram(map->track_frag_shader);
	if (map->track_program) glDeleteProgram(map->track_program);
	if (map->tex_array) glDeleteTextures(1, &map->tex_array);
	if (map->track_vao) glDeleteTextures(1, &map->track_vao);
	if (map->track_vbo) glDeleteTextures(1, &map->track_vbo);

}

void
gl_openstreetmap_raster(GLOpenStreetMap *map, int width, int height, float x_center, float y_center, float zoom)
{
	const float digital_zoom = powf(2, zoom - mipmap(zoom));
	const int x_size = 1 << mipmap(zoom);
	const int y_size = 1 << mipmap(zoom);
	/* Enough to fill the screen plus a 1x border all around for safety */
	const int x_count = MIN(x_size, ceilf((float)width / MAP_TILE_WIDTH / digital_zoom) + 2);
	const int y_count = MIN(y_size, ceilf((float)height / MAP_TILE_HEIGHT / digital_zoom) + 2);
	const int count = x_count * y_count;
	const float track_color[] = STYLE_ACCENT_1_NORMALIZED;
	int x_start, y_start;
	int i;

	GLfloat proj[4][4] = {
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	};

	x_center *= x_size;
	y_center *= y_size;

	x_start = (int)roundf(x_center - x_count/2);
	y_start = (int)roundf(y_center - y_count/2);

	/**
	 * Projection transform:
	 * - Pan so that (lat,lon) is at (0,0)
	 * - Scale so that 1 texture pixel = 1 viewport pixel
	 * - Scale by the delta between zoom and mipmap zoom
	 * - Scale by width/height to go from pixel coords to viewport coords
	 */
	proj[0][0] *= 2.0f * MAP_TILE_WIDTH / (float)width * digital_zoom;
	proj[1][1] *= 2.0f * -MAP_TILE_HEIGHT / (float)height * digital_zoom;
	proj[3][0] = proj[0][0] * (- x_center);
	proj[3][1] = proj[1][1] * (- y_center);


	/* Resize array to fit all tiles */
	glBindVertexArray(map->vao);
	if (map->tile_count < count) {
		map->tiles = reallocarray(map->tiles, count, sizeof(*map->tiles));

		/* Have to start from scratch since the texture 2d array will be deleted */
		for (i=map->tile_count; i<count; i++) {
			map->tiles[i].in_use = 0;
			map->tiles[i].x = -1;
			map->tiles[i].y = -1;

			glGenBuffers(1, &map->tiles[i].vbo);
			glGenBuffers(1, &map->tiles[i].ibo);
		}

#ifndef NDEBUG
		printf("Tile count: %d -> %d\n", map->tile_count, count);
#endif
		map->tile_count = count;
	}

	/* Load missing vertex buffers */
	update_buffers(map, x_start, y_start, x_count, y_count, mipmap(zoom));


	/* Setup program state */
	glUseProgram(map->texture_program);
	glBindVertexArray(map->vao);
	glUniformMatrix4fv(map->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);

	/* Draw map */
#ifndef NDEBUG
	int call_count = 0;
#endif
	glBindVertexArray(map->vao);
	for (i=0; i<map->tile_count; i++) {
		if (map->tiles[i].in_use) {
#ifndef NDEBUG
			call_count++;
#endif
			/* Bind vbo & ibo */
			glBindBuffer(GL_ARRAY_BUFFER, map->tiles[i].vbo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->tiles[i].ibo);

			glEnableVertexAttribArray(map->attrib_pos);
			glVertexAttribPointer(map->attrib_pos, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));


			/* Draw */
			glDrawElements(GL_LINES, map->tiles[i].vertex_count, GL_UNSIGNED_SHORT, 0);
		}
	}
#ifndef NDEBUG
	printf("Total draw calls (map): %d\n", call_count);
#endif


	/* Swap to the track program */
	glUseProgram(map->track_program);
	glBindVertexArray(map->track_vao);
	glBindBuffer(GL_ARRAY_BUFFER, map->track_vbo);
	glBufferData(GL_ARRAY_BUFFER, get_data_count() * sizeof(GeoPoint), get_track_data(), GL_STREAM_DRAW);

	/* Shader converts (lat,lon) to (x,y): translate so that the center of the
	 * viewport is (0, 0) */
	proj[3][0] = proj[0][0] * -x_center;
	proj[3][1] = proj[1][1] * -y_center;

	glUniformMatrix4fv(map->u4m_track_proj, 1, GL_FALSE, (GLfloat*)proj);
	glUniform4fv(map->u4f_track_color, 1, track_color);
	glUniform1f(map->u1f_zoom, 1 << mipmap(zoom));

	glDrawArrays(GL_POINTS, 0, get_data_count());

	/* Cleanup */
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    //glDisable(GL_BLEND);
}

/* Static functions {{{ */
static void
map_opengl_init(GLOpenStreetMap *map)
{
	GLint status;

	static const GLchar *vertex_shader =
		SHADER_VERSION
        "precision mediump float;\n"

		"uniform mat4 ProjMtx;\n"

		"in vec2 Position;\n"

		"void main() {\n"
		"   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
		//"   gl_Position = vec4(Position.xy, 0, 1);\n"
		"}";
	static const GLchar *fragment_shader =
		SHADER_VERSION
        "precision mediump float;\n"

		"out vec4 Out_Color;\n"

		"void main() {\n"
		"   Out_Color = vec4(0.23, 0.52, 0.88, 1);\n"
		"}";

	/* Program + shaders */
	map->texture_program = glCreateProgram();
	map->tex_vert_shader = glCreateShader(GL_VERTEX_SHADER);
	map->tex_frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(map->tex_vert_shader, 1, &vertex_shader, 0);
	glShaderSource(map->tex_frag_shader, 1, &fragment_shader, 0);

	glCompileShader(map->tex_vert_shader);
	glGetShaderiv(map->tex_vert_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glCompileShader(map->tex_frag_shader);
	glGetShaderiv(map->tex_frag_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glAttachShader(map->texture_program, map->tex_vert_shader);
	glAttachShader(map->texture_program, map->tex_frag_shader);
	glLinkProgram(map->texture_program);

	glGetProgramiv(map->texture_program, GL_LINK_STATUS, &status);
	assert(status == GL_TRUE);

	/* Uniforms + attributes */
	map->u4m_proj = glGetUniformLocation(map->texture_program, "ProjMtx");
	map->attrib_pos = glGetAttribLocation(map->texture_program, "Position");

	/* Buffers, arrays, and layouts */
	glGenVertexArrays(1, &map->vao);
	glBindVertexArray(map->vao);

	glEnableVertexAttribArray(map->attrib_pos);
	glVertexAttribPointer(map->attrib_pos, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
}

static void
track_opengl_init(GLOpenStreetMap *map)
{
	GLuint attrib_track_pos;
	GLint status;

	static const GLchar *vertex_shader =
		SHADER_VERSION
        "precision mediump float;\n"

		"uniform mat4 ProjMtx;\n"
		"uniform float Zoom;\n"

		"in vec2 Position;\n"

		"void main() {\n"
		"   float pi = 3.1415926536;\n"
		"   float rads = Position.x * pi / 180.0;\n"
		"   float x = Zoom * (Position.y + 180.0) / 360.0;\n"
		"   float y = Zoom * (1.0 - log(tan(rads) + 1.0/cos(rads)) / pi) / 2.0;\n"
		"   gl_Position = ProjMtx * vec4(x, y, 0, 1);\n"
		"   gl_PointSize = 4.0f;\n"
		//"   gl_Position = vec4(Position.xy, 0, 1);\n"
		"}";
	static const GLchar *fragment_shader =
		SHADER_VERSION
		"precision mediump float;\n"

		"uniform vec4 TrackColor;\n"

		"out vec4 Out_Color;\n"

		"void main() {\n"
		"   Out_Color = TrackColor;\n"
		"}";

	/* Program + shaders */
	map->track_program = glCreateProgram();
	map->track_vert_shader = glCreateShader(GL_VERTEX_SHADER);
	map->track_frag_shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(map->track_vert_shader, 1, &vertex_shader, 0);
	glShaderSource(map->track_frag_shader, 1, &fragment_shader, 0);

	glCompileShader(map->track_vert_shader);
	glGetShaderiv(map->track_vert_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glCompileShader(map->track_frag_shader);
	glGetShaderiv(map->track_frag_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glAttachShader(map->track_program, map->track_vert_shader);
	glAttachShader(map->track_program, map->track_frag_shader);
	glLinkProgram(map->track_program);

	glGetProgramiv(map->track_program, GL_LINK_STATUS, &status);
	assert(status == GL_TRUE);

	/* Uniforms + attributes */
	map->u4m_track_proj = glGetUniformLocation(map->track_program, "ProjMtx");
	map->u4f_track_color = glGetUniformLocation(map->track_program, "TrackColor");
	map->u1f_zoom = glGetUniformLocation(map->track_program, "Zoom");
	attrib_track_pos = glGetAttribLocation(map->track_program, "Position");

	glGenVertexArrays(1, &map->track_vao);
	glBindVertexArray(map->track_vao);
	glGenBuffers(1, &map->track_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, map->track_vbo);

	glEnableVertexAttribArray(attrib_track_pos);
	glVertexAttribPointer(attrib_track_pos, 2, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, lat));


}

static void
update_buffers(GLOpenStreetMap *map, int x_start, int y_start, int x_count, int y_count, int zoom)
{
	const int x_size = 1 << zoom;
	const int y_size = 1 << zoom;
	const int x_end = x_start + x_count;
	const int y_end = y_start + y_count;
	FILE *fd;
	int i, x, y, actual_x, actual_y;
	char path[256];
	void *tiledata;
	uint32_t buflen;

	/* Mark textures that are no longer in use as replaceable (with wraparounds) */
	for (i=0; i<map->tile_count; i++) {
		if (map->tiles[i].in_use && (
			map->tiles[i].x < x_start
			|| map->tiles[i].x >= x_end
			|| map->tiles[i].y < y_start
			|| map->tiles[i].y >= y_end)) {
#ifndef NDEBUG
			printf("[%d,%d] Tile (%d,%d) marked for removal\n", x_start, y_start, map->tiles[i].x, map->tiles[i].y);
#endif

			map->tiles[i].in_use = 0;
		}
	}


	/* Load buffers from file */
	for (x = 0; x < x_count; x++) {
		actual_x = x_start + x;
		for (y = 0; y < y_count; y++) {
			actual_y = y_start + y;

			/* If outside x/y bounds, bind quads to texture -1 aka draw black */
			if (actual_y < 0 || actual_y >= y_size
			 || actual_x < 0 || actual_x >= x_size) {
				i = -1;
			} else {

				/* Check if tile is already in GPU memory */
				for (i=0; i<map->tile_count; i++) {
					if (map->tiles[i].x == actual_x && map->tiles[i].y == actual_y) {
						map->tiles[i].in_use = 1;
						break;
					}
				}

				if (i == map->tile_count) {
					/* Tile is not in memory yet: find an empty slot */
					for (i=0; i<map->tile_count && map->tiles[i].in_use; i++)
						;

					/* Update tile metadata */
					map->tiles[i].x = actual_x;
					map->tiles[i].y = actual_y;
					map->tiles[i].in_use = 1;

					/* Load new tile from file */
					sprintf(path, "/home/dbdexter/Projects/magellan/binary_svgs/%d_%d_0.bin", actual_x, actual_y);
#ifndef NDEBUG
					printf("Loading %s into %d\n", path, i);
#endif
					fd = fopen(path, "rb");

					if (!fd) {
						/* Nothing in this tile */
						glBindVertexArray(map->vao);
						glBindBuffer(GL_ARRAY_BUFFER, map->tiles[i].vbo);
						glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->tiles[i].ibo);

						glBufferData(GL_ARRAY_BUFFER, 0, tiledata, GL_STATIC_DRAW);
						glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, tiledata, GL_STATIC_DRAW);
					} else {
						/* Bind vbo & ibo for the tile */
						glBindVertexArray(map->vao);
						glBindBuffer(GL_ARRAY_BUFFER, map->tiles[i].vbo);
						glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->tiles[i].ibo);

#ifndef NDEBUG
						printf("Uploading [%d,%d] to VRAM", actual_x, actual_y);
#endif

						/* Read and upload vbo */
						fread(&buflen, 4, 1, fd);
						tiledata = malloc(buflen * sizeof(Vertex));
						fread(tiledata, buflen, sizeof(Vertex), fd);

						glBufferData(GL_ARRAY_BUFFER, buflen * sizeof(Vertex), tiledata, GL_STATIC_DRAW);
						free(tiledata);

						/* Read and upload ibo */
						fread(&buflen, 4, 1, fd);
						tiledata = malloc(buflen * sizeof(uint16_t));
						fread(tiledata, buflen, sizeof(uint16_t), fd);
						glBufferData(GL_ELEMENT_ARRAY_BUFFER, buflen * sizeof(uint16_t), tiledata, GL_STATIC_DRAW);
						free(tiledata);

						map->tiles[i].vertex_count = buflen;

						fclose(fd);

#ifndef NDEBUG
						printf("Loaded tile (%d,%d) -> [%d]\n", actual_x, actual_y, i);
#endif
					}
				}
			}
		}
	}
}

static int
mipmap(float zoom)
{
	return 6;
}

/* }}} */
