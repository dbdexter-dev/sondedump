#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include <GLES3/gl3.h>
#include "decode.h"
#include "gl_map.h"
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

/* Tile data and index will be linked into the final executable */
extern const char _binary_tiledata_bin_start[];
extern const char _binary_tileindex_bin_start[];

static void map_opengl_init(GLMap *map);
static void track_opengl_init(GLMap *map);
static int mipmap(float zoom);

static void update_buffers(GLMap *map, int x_start, int y_start, int x_count, int y_count, int zoom);


void
gl_map_init(GLMap *map)
{
	/* Background map program, buffers, uniforms... */
	map_opengl_init(map);

	/* Ground track program, buffers, uniforms... */
	track_opengl_init(map);
}

void
gl_map_deinit(GLMap *map)
{
	if (map->vao) glDeleteVertexArrays(1, &map->vao);
	if (map->vbo) glDeleteBuffers(1, &map->vbo);
	if (map->ibo) glDeleteBuffers(1, &map->ibo);
	if (map->tile_vert_shader) glDeleteProgram(map->tile_vert_shader);
	if (map->tile_frag_shader) glDeleteProgram(map->tile_frag_shader);
	if (map->tile_program) glDeleteProgram(map->tile_program);

	if (map->track_vao) glDeleteVertexArrays(1, &map->track_vao);
	if (map->track_vbo) glDeleteBuffers(1, &map->track_vbo);
	if (map->track_vert_shader) glDeleteProgram(map->track_vert_shader);
	if (map->track_frag_shader) glDeleteProgram(map->track_frag_shader);
	if (map->track_program) glDeleteProgram(map->track_program);
}

void
gl_map_vector(GLMap *map, int width, int height, float x_center, float y_center, float zoom)
{
	const float digital_zoom = powf(2, zoom - mipmap(zoom));
	const int x_size = 1 << mipmap(zoom);
	const int y_size = 1 << mipmap(zoom);
	/* Enough to fill the screen plus a 1x border all around for safety */
	const int x_count = ceilf((float)width / MAP_TILE_WIDTH / digital_zoom) + 2;
	const int y_count = ceilf((float)height / MAP_TILE_HEIGHT / digital_zoom) + 2;
	const float track_color[] = STYLE_ACCENT_1_NORMALIZED;
	const float map_color[] = STYLE_ACCENT_0_NORMALIZED;
	int x_start, y_start;

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
	 * - Scale so that 1 tile pixel = 1 viewport pixel
	 * - Scale by the delta between zoom and mipmap zoom
	 * - Scale by width/height to go from pixel coords to viewport coords
	 */
	proj[0][0] *= 2.0f * MAP_TILE_WIDTH / (float)width * digital_zoom;
	proj[1][1] *= 2.0f * -MAP_TILE_HEIGHT / (float)height * digital_zoom;
	proj[3][0] = proj[0][0] * (-x_center);
	proj[3][1] = proj[1][1] * (-y_center);

	/* Resize array to fit all tiles */
	glBindVertexArray(map->vao);

	/* Load missing vertex buffers */
	update_buffers(map, x_start, y_start, x_count, y_count, mipmap(zoom));

	/* Draw map {{{ */
	glUseProgram(map->tile_program);
	glBindVertexArray(map->vao);
	glUniformMatrix4fv(map->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);

	glBindVertexArray(map->vao);
	glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->ibo);

	glUniform4fv(map->u4f_map_color, 1, map_color);
	glDrawElements(GL_LINES, map->vram_tile_metadata.vertex_count, GL_UNSIGNED_INT, 0);
	/* }}} */

	/* Draw ground track {{{ */
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
	/* }}} */

	/* Cleanup */
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    //glDisable(GL_BLEND);
}

/* Static functions {{{ */
static void
update_buffers(GLMap *map, int x_start, int y_start, int x_count, int y_count, int zoom)
{
	const int x_size = 1 << zoom;
	const int y_size = 1 << zoom;
	int i, x, y, actual_x, actual_y;

	Vertex *vbo_data;
	uint32_t *ibo_data;
	uint16_t *packed_ibo_data = NULL;
	size_t vbo_len = 0, ibo_len = 0, prev_ibo_len = 0;
	uint32_t len;
	uint32_t max_ibo, ibo_offset;
	int changed;
	int index_offset;
	int binary_offset;

	/* If the viewport has not changed, exit */
	if (MAX(0, x_start) == map->vram_tile_metadata.x_start &&
	    MAX(0, y_start) == map->vram_tile_metadata.y_start &&
	    MIN(x_size, x_count) == map->vram_tile_metadata.x_count &&
	    MIN(y_size, y_count) == map->vram_tile_metadata.y_count &&
	    zoom == map->vram_tile_metadata.zoom) {
		return;
	}

#ifndef NDEBUG
	printf("Detected viewport change to %dx%d\n", x_count, y_count);
#endif

	map->vram_tile_metadata.x_start = MAX(0, x_start);
	map->vram_tile_metadata.y_start = MAX(0, y_start);
	map->vram_tile_metadata.x_count = MIN(x_size, x_count);
	map->vram_tile_metadata.y_count = MIN(y_size, y_count);
	map->vram_tile_metadata.zoom = zoom;

	vbo_data = NULL;
	ibo_data = NULL;
	max_ibo = 0;
	ibo_offset = 0;
	changed = 0;

	/* Load all tiles into RAM */
	for (x = 0; x < x_count; x++) {
		actual_x = x_start + x;
		for (y = 0; y < y_count; y++) {
			actual_y = y_start + y;

			/* If outside x/y bounds, draw as black and go to the next */
			if (actual_y < 0 || actual_y >= y_size
			 || actual_x < 0 || actual_x >= x_size) {
				continue;
			}

#ifndef NDEBUG
			printf("Attempting to load (%d,%d)...\n", actual_x, actual_y);
#endif

			changed = 1;

			/* Compute start of vbo/ibo data */
			index_offset = actual_x * y_size + actual_y;
			binary_offset = ((uint32_t*)_binary_tileindex_bin_start)[index_offset];

			/* If tile is unavailable, draw as black and go to the next */
			if (binary_offset < 0) continue;

			/* Copy vbo data */
			len = *(uint32_t*)(_binary_tiledata_bin_start + binary_offset);
			binary_offset += 4;

			vbo_data = realloc(vbo_data, (vbo_len + len) * sizeof(*vbo_data));
			memcpy(vbo_data + vbo_len, _binary_tiledata_bin_start + binary_offset, len * sizeof(*vbo_data));
			vbo_len += len;
			binary_offset += sizeof(*vbo_data) * len;

			/* Read and unpack ibo data */
			len = *(uint32_t*)(_binary_tiledata_bin_start + binary_offset);
			binary_offset += 4;

			packed_ibo_data = (uint16_t*)(_binary_tiledata_bin_start + binary_offset);

			ibo_data = realloc(ibo_data, (ibo_len + len) * sizeof(*ibo_data));
			for (i=0; i < (int)len; i++) {
				ibo_data[ibo_len + i] = (uint32_t)packed_ibo_data[i] + ibo_offset;
				max_ibo = MAX(ibo_data[ibo_len + i], max_ibo);
			}

			ibo_len += len;
			ibo_offset = max_ibo + 1;
#ifndef NDEBUG
			printf("New IBO offset: %u\n", ibo_offset);
#endif
		}
	}

	map->vram_tile_metadata.vertex_count = ibo_len - prev_ibo_len;
	prev_ibo_len = ibo_len;

	if (changed) {
		glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->ibo);
		glBufferData(GL_ARRAY_BUFFER, vbo_len * sizeof(*vbo_data), vbo_data, GL_STATIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibo_len * sizeof(*ibo_data), ibo_data, GL_STATIC_DRAW);
	}

	free(vbo_data);
	free(ibo_data);
}


static int
mipmap(float zoom)
{
	(void)zoom;
	/* No matter the zoom level, we're using the same perceived zoom */
	return 6;
}

static void
map_opengl_init(GLMap *map)
{
	GLint status;

	static const GLchar *vertex_shader =
		SHADER_VERSION
        "precision mediump float;\n"

		"uniform mat4 ProjMtx;\n"

		"in vec2 Position;\n"

		"void main() {\n"
		"   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
		"}";
	static const GLchar *fragment_shader =
		SHADER_VERSION
        "precision mediump float;\n"

		"uniform vec4 MapColor;\n"
		"out vec4 Out_Color;\n"

		"void main() {\n"
		"   Out_Color = MapColor;\n"
		"}";

	/* Program + shaders */
	map->tile_program = glCreateProgram();
	map->tile_vert_shader = glCreateShader(GL_VERTEX_SHADER);
	map->tile_frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(map->tile_vert_shader, 1, &vertex_shader, 0);
	glShaderSource(map->tile_frag_shader, 1, &fragment_shader, 0);

	glCompileShader(map->tile_vert_shader);
	glGetShaderiv(map->tile_vert_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glCompileShader(map->tile_frag_shader);
	glGetShaderiv(map->tile_frag_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glAttachShader(map->tile_program, map->tile_vert_shader);
	glAttachShader(map->tile_program, map->tile_frag_shader);
	glLinkProgram(map->tile_program);

	glGetProgramiv(map->tile_program, GL_LINK_STATUS, &status);
	assert(status == GL_TRUE);

	/* Uniforms + attributes */
	map->u4m_proj = glGetUniformLocation(map->tile_program, "ProjMtx");
	map->u4f_map_color = glGetUniformLocation(map->tile_program, "MapColor");
	map->attrib_pos = glGetAttribLocation(map->tile_program, "Position");

	/* Buffers, arrays, and layouts */
	glGenVertexArrays(1, &map->vao);
	glBindVertexArray(map->vao);
	glGenBuffers(1, &map->vbo);
	glGenBuffers(1, &map->ibo);

	glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->ibo);

	glEnableVertexAttribArray(map->attrib_pos);
	glVertexAttribPointer(map->attrib_pos, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));

	map->vram_tile_metadata.x_start = 0;
	map->vram_tile_metadata.y_start = 0;
	map->vram_tile_metadata.x_count = 0;
	map->vram_tile_metadata.y_count = 0;
	map->vram_tile_metadata.zoom = 0;
}

static void
track_opengl_init(GLMap *map)
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
/* }}} */
