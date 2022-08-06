#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "libs/glad/glad.h"
#include "gui/gl_utils.h"
#include "decode.h"
#include "gl_map.h"
#include "log/log.h"
#include "style.h"
#include "utils.h"
#include "shaders/shaders.h"

#define MODLT(x, y, mod) (((y) - (x) - 1 + (mod)) % (mod) < ((mod) / 2))
#define MODGE(x, y, mod) (((x) - (y) + (mod)) % (mod) < ((mod) / 2))

typedef struct {
	float x, y;
} Vertex;

/* Tile data and index will be linked into the final executable */
extern const char _binary_tiledata_bin[];
extern const unsigned long _binary_tiledata_bin_size;
extern const char _binary_tileindex_bin[];
extern const unsigned long _binary_tileindex_bin_size;

static void vector_map_opengl_init(GLMap *map);
static void update_buffers(GLMap *ctx, int x_start, int y_start, int x_count, int y_count, int zoom);
static int mipmap(float zoom);


void
gl_map_init(GLMap *ctx)
{
	/* Background map program, buffers, uniforms... */
	vector_map_opengl_init(ctx);
}

void
gl_map_deinit(GLMap *ctx)
{
	if (ctx->vao) glDeleteVertexArrays(1, &ctx->vao);
	if (ctx->vbo) glDeleteBuffers(1, &ctx->vbo);
	if (ctx->ibo) glDeleteBuffers(1, &ctx->ibo);
	if (ctx->tile_program) glDeleteProgram(ctx->tile_program);
}

void
gl_map_vector(GLMap *ctx, const Config *conf, int width, int height)
{
	float center_x = conf->map.center_x;
	float center_y = conf->map.center_y;
	float zoom = conf->map.zoom;

	const float digital_zoom = powf(2, zoom - mipmap(zoom));
	const int x_size = 1 << mipmap(zoom);
	const int y_size = 1 << mipmap(zoom);
	/* Enough to fill the screen plus a 1x border all around for safety */
	const int x_count = ceilf((float)width / MAP_TILE_WIDTH / digital_zoom) + 2;
	const int y_count = ceilf((float)height / MAP_TILE_HEIGHT / digital_zoom) + 2;
	const float map_color[] = STYLE_ACCENT_0_NORMALIZED;
	int x_start, y_start;

	GLfloat proj[4][4] = {
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	};

	center_x *= x_size;
	center_y *= y_size;

	x_start = (int)roundf(center_x - x_count/2);
	y_start = (int)roundf(center_y - y_count/2);

	/**
	 * Projection transform:
	 * - Pan so that (lat,lon) is at (0,0)
	 * - Scale so that 1 tile pixel = 1 viewport pixel
	 * - Scale by the delta between zoom and mipmap zoom
	 * - Scale by width/height to go from pixel coords to viewport coords
	 */
	proj[0][0] *= 2.0f * MAP_TILE_WIDTH / (float)width * digital_zoom;
	proj[1][1] *= 2.0f * -MAP_TILE_HEIGHT / (float)height * digital_zoom;
	proj[3][0] = proj[0][0] * (-center_x);
	proj[3][1] = proj[1][1] * (-center_y);

	/* Load missing vertex buffers */
	update_buffers(ctx, x_start, y_start, x_count, y_count, mipmap(zoom));

	/* Draw map {{{ */
	glUseProgram(ctx->tile_program);
	glBindVertexArray(ctx->vao);
	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);

	glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
	glUniform4fv(ctx->u4f_map_color, 1, map_color);
	glDrawElements(GL_LINE_STRIP, ctx->vram_tile_metadata.vertex_count, GL_UNSIGNED_INT, 0);
	glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
	/* }}} */

	/* Cleanup */
	glUseProgram(0);
	glBindVertexArray(0);
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

	log_debug("Detected viewport change to %dx%d", x_count, y_count);

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

			changed = 1;

			/* Compute start of vbo/ibo data */
			index_offset = actual_x * y_size + actual_y;
			binary_offset = ((uint32_t*)_binary_tileindex_bin)[index_offset];

			/* If tile is unavailable, draw as black and go to the next */
			if (binary_offset < 0) continue;

			/* Copy vbo data */
			len = *(uint32_t*)(_binary_tiledata_bin + binary_offset);
			binary_offset += 4;

			vbo_data = realloc(vbo_data, (vbo_len + len) * sizeof(*vbo_data));
			memcpy(vbo_data + vbo_len, _binary_tiledata_bin + binary_offset, len * sizeof(*vbo_data));
			vbo_len += len;
			binary_offset += sizeof(*vbo_data) * len;

			/* Read and unpack ibo data */
			len = *(uint32_t*)(_binary_tiledata_bin + binary_offset);
			binary_offset += 4;

			packed_ibo_data = (uint16_t*)(_binary_tiledata_bin + binary_offset);

			ibo_data = realloc(ibo_data, (ibo_len + len) * sizeof(*ibo_data));
			for (i=0; i < (int)len; i++) {
				if (packed_ibo_data[i] == 0xFFFF) {
					/* Extend restart index to 32 bits */
					ibo_data[ibo_len + i] = 0xFFFFFFFF;
				} else {
					/* Extend regular index to 32 bits */
					ibo_data[ibo_len + i] = (uint32_t)packed_ibo_data[i] + ibo_offset;
					max_ibo = MAX(ibo_data[ibo_len + i], max_ibo);
				}
			}

			ibo_len += len;
			ibo_offset = max_ibo + 1;
		}
	}

	map->vram_tile_metadata.vertex_count = ibo_len - prev_ibo_len;
	prev_ibo_len = ibo_len;

	if (changed) {
		glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->ibo);
		glBufferData(GL_ARRAY_BUFFER, vbo_len * sizeof(*vbo_data), vbo_data, GL_STATIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibo_len * sizeof(*ibo_data), ibo_data, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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
vector_map_opengl_init(GLMap *map)
{
	GLint status, attrib_pos_x, attrib_pos_y;

	const GLchar *vertex_shader = _binary_simpleproj_vert;
	const int vertex_shader_len = SYMSIZE(_binary_simpleproj_vert);
	const GLchar *fragment_shader = _binary_simplecolor_frag;
	const int fragment_shader_len = SYMSIZE(_binary_simplecolor_frag);

	/* Program + shaders */
	status = gl_compile_and_link(&map->tile_program,
	                             vertex_shader, vertex_shader_len,
	                             fragment_shader, fragment_shader_len);
	if (status != GL_TRUE) log_error("Failed to compile shaders");

	/* Uniforms + attributes */
	map->u4m_proj = glGetUniformLocation(map->tile_program, "u_proj_mtx");
	map->u4f_map_color = glGetUniformLocation(map->tile_program, "u_color");
	attrib_pos_x = glGetAttribLocation(map->tile_program, "in_position_x");
	attrib_pos_y = glGetAttribLocation(map->tile_program, "in_position_y");

	log_debug("proj_mtx %d color %d", map->u4m_proj, map->u4f_map_color);
	log_debug("pos_x %d pos_y %d", attrib_pos_x, attrib_pos_y);

	/* Buffers, arrays, and layouts */
	glGenVertexArrays(1, &map->vao);
	glBindVertexArray(map->vao);
	glGenBuffers(1, &map->vbo);
	glGenBuffers(1, &map->ibo);

	glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->ibo);

	glEnableVertexAttribArray(attrib_pos_x);
	glEnableVertexAttribArray(attrib_pos_y);
	glVertexAttribPointer(attrib_pos_x, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
	glVertexAttribPointer(attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, y));

	memset(&map->vram_tile_metadata, 0, sizeof(map->vram_tile_metadata));
}
/* }}} */
