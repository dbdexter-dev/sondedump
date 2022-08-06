#include <assert.h>
#include <math.h>
#include <stddef.h>
#include "gui/shaders/shaders.h"
#include "gui/stb_image/stb_image.h"
#include "gui/gl_utils.h"
#include "gl_map_raster.h"
#include "log/log.h"
#include "utils.h"

#define MAP_TILE_WIDTH 256
#define MAP_TILE_HEIGHT 256
#define MAX_OVERZOOM 2
#define MAX_ZOOM_LEVEL 16

typedef struct {
	float x, y;
	float tex_u, tex_v;
	int tex_id;
} Vertex;

static void raster_map_init(GLRasterMap *ctx);
static void build_quads(int x_count, int y_count, Vertex *vertices, unsigned int *indices);
static void update_textures(GLRasterMap *ctx, const Config *conf, Vertex *vertices, int x_start, int x_count, int y_start, int y_count, int zoom);
static void resize_texture(GLuint *texture, int count);
static int mipmap(float zoom);

void
gl_raster_map_init(GLRasterMap *ctx)
{
	raster_map_init(ctx);
	tilecache_init(&ctx->cache, CACHE_SIZE);

	ctx->vram.tex = 0;
	ctx->vram.metadata = NULL;
	ctx->vram.count = 0;
}

void
gl_raster_map_deinit(GLRasterMap *ctx)
{
	if (ctx->program) glDeleteProgram(ctx->program);
	if (ctx->vao) glDeleteVertexArrays(1, &ctx->vao);
	if (ctx->vbo) glDeleteBuffers(1, &ctx->vbo);
	if (ctx->ibo) glDeleteBuffers(1, &ctx->ibo);

	tilecache_deinit(&ctx->cache);
	free(ctx->vram.metadata);
}

void
gl_raster_map_flush(GLRasterMap *ctx)
{
	tilecache_flush(&ctx->cache);
}

void
gl_map_raster(GLRasterMap *ctx, const Config *conf, int width, int height)
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
	const int count = x_count * y_count;
	int x_start, y_start;

	Vertex *vertices;
	unsigned int *indices;

	center_x *= x_size;
	center_y *= y_size;

	x_start = (int)floorf(center_x - x_count/2.0);
	y_start = (int)floorf(center_y - y_count/2.0);

	GLfloat proj[4][4] = {
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	};

	/**
	 * Projection transform:
	 * - Pan so that (lat,lon) is at (0,0)
	 * - Scale so that 1 texture pixel = 1 viewport pixel
	 * - Scale by the delta between zoom and mipmap zoom
	 * - Scale by width/height to go from pixel coords to viewport coords
	 */
	proj[0][0] *= 2.0f * MAP_TILE_WIDTH / (float)width * digital_zoom;
	proj[1][1] *= -2.0f * MAP_TILE_HEIGHT / (float)height * digital_zoom;
	proj[3][0] = proj[0][0] * (x_start - center_x);
	proj[3][1] = proj[1][1] * (y_start - center_y);

	/* Setup quads */
	vertices = malloc(sizeof(*vertices) * 4 * count);
	indices = malloc(sizeof(*indices) * 6 * count);
	build_quads(x_count, y_count, vertices, indices);

	/* Update texture bindings */
	update_textures(ctx, conf, vertices, x_start, x_count, y_start, y_count, mipmap(zoom));

	/* Render quads {{{ */
	glUseProgram(ctx->program);
	glBindVertexArray(ctx->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ibo);

	glBufferData(GL_ARRAY_BUFFER, 4 * count * sizeof(*vertices), vertices, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * count * sizeof(*indices), indices, GL_STATIC_DRAW);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, ctx->vram.tex);
	glUniform1i(ctx->u1i_texture, 0);
	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);

	glDrawElements(GL_TRIANGLES, 6 * count, GL_UNSIGNED_INT, 0);
	/* }}} */

	free(vertices);
	free(indices);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);
}

/* Static functions {{{ */
static void
raster_map_init(GLRasterMap *ctx)
{
	GLint status;
	GLint attrib_position, attrib_texuv, attrib_texid;

	const GLchar *vertex_shader = _binary_texture_vert;
	const int vertex_shader_len = SYMSIZE(_binary_texture_vert);
	const GLchar *fragment_shader = _binary_texture_frag;
	const int fragment_shader_len = SYMSIZE(_binary_texture_frag);

	/* Program + shaders */
	status = gl_compile_and_link(&ctx->program,
	                             vertex_shader, vertex_shader_len,
	                             fragment_shader, fragment_shader_len);
	if (status != GL_TRUE) log_error("Failed to compile shaders");

	/* Uniforms + attributes */
	ctx->u4m_proj = glGetUniformLocation(ctx->program, "u_proj_mtx");
	ctx->u1i_texture = glGetUniformLocation(ctx->program, "u_texture");
	attrib_position = glGetAttribLocation(ctx->program, "in_position");
	attrib_texuv = glGetAttribLocation(ctx->program, "in_texUV");
	attrib_texid = glGetAttribLocation(ctx->program, "in_texID");

	log_debug("proj_mtx %d color %d", ctx->u4m_proj);
	log_debug("position %d uv %d id %d", attrib_position, attrib_texuv, attrib_texid);

	/* Buffers, arrays, and layouts */
	glGenVertexArrays(1, &ctx->vao);
	glBindVertexArray(ctx->vao);
	glGenBuffers(1, &ctx->vbo);
	glGenBuffers(1, &ctx->ibo);

	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ibo);

	glEnableVertexAttribArray(attrib_position);
	glEnableVertexAttribArray(attrib_texuv);
	glEnableVertexAttribArray(attrib_texid);
	glVertexAttribPointer(attrib_position, 2, GL_FLOAT, GL_FALSE,
	                      sizeof(Vertex), (void*)offsetof(Vertex, x));
	glVertexAttribPointer(attrib_texuv, 2, GL_FLOAT, GL_FALSE,
	                      sizeof(Vertex), (void*)offsetof(Vertex, tex_u));
	glVertexAttribIPointer(attrib_texid, 1, GL_INT,
	                      sizeof(Vertex), (void*)offsetof(Vertex, tex_id));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

static void
build_quads(int x_count, int y_count, Vertex *vertices, unsigned int *indices)
{
	int x, y, i, idx;

	for (x = 0; x < x_count; x++) {
		for (y = 0; y < y_count; y++) {
			/* 4 vertices */
			for (i=0; i<4; i++) {
				idx = i + y * 4 + x * 4 * y_count;

				vertices[idx].x = x + i/2;
				vertices[idx].y = y + i%2;
			}

			/* 2x triangles (3 indices each) */
			idx = y * 6 + x * 6 * y_count;

			indices[idx]   = y * 4 + x * 4 * y_count;
			indices[idx+1] = y * 4 + x * 4 * y_count + 1;
			indices[idx+2] = y * 4 + x * 4 * y_count + 2;
			indices[idx+3] = y * 4 + x * 4 * y_count + 1;
			indices[idx+4] = y * 4 + x * 4 * y_count + 2;
			indices[idx+5] = y * 4 + x * 4 * y_count + 3;
		}
	}
}

static void
update_textures(GLRasterMap *ctx, const Config *conf, Vertex *vertices, int x_start, int x_count, int y_start, int y_count, int zoom)
{
	const int new_count = x_count * y_count;
	int world_x, world_y;
	int i, x, y;
	int tex_id, vertex_start;
	float texuv_offset_u, texuv_offset_v;
	int texuv_divisor;
	const uint8_t *pngdata;
	uint8_t *imgdata;
	size_t pnglen;
	int img_w, img_h, img_n;
	int local_wx, local_wy, local_zoom;

	/* If the number of textures has increased */
	if (new_count > ctx->vram.count) {
		log_debug("Texture count changed: %d -> %d", ctx->vram.count, new_count);

		/* Resize the texture to fit, marking everything as unused since we have
		 * to reupload all the textures anyway */
		free(ctx->vram.metadata);
		ctx->vram.metadata = calloc(new_count, sizeof(*ctx->vram.metadata));
		resize_texture(&ctx->vram.tex, new_count);
		ctx->vram.count = new_count;
	} else {
		/* Mark textures outside of screen bounds as unused */
		for (i=0; i<ctx->vram.count; i++) {
			if ((unsigned)(ctx->vram.metadata[i].x - x_start) >= (unsigned)x_count
			 || (unsigned)(ctx->vram.metadata[i].y - y_start) >= (unsigned)y_count
			 || ctx->vram.metadata[i].z != zoom) {
				ctx->vram.metadata[i].in_use = 0;
			}
		}

	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, ctx->vram.tex);

	/* For each texture that should be loaded */
	for (x=0; x < x_count; x++) {
		world_x = x_start + x;
		for (y=0; y < y_count; y++) {
			world_y = y_start + y;

			/* By default, the texture bounds coincide with the quad bounds */
			texuv_divisor = 1;
			texuv_offset_u = 0;
			texuv_offset_v = 0;
			tex_id = -1;

			/* If the texture is out of bounds, draw it black */
			if (world_x < 0 || world_x >= (1 << zoom) || world_y < 0 || world_y >= (1 << zoom)) {
				goto assign_texture;
			}

			local_wx = world_x;
			local_wy = world_y;

			for (local_zoom = zoom; local_zoom >= MAX(zoom - MAX_OVERZOOM, 0); local_zoom--) {
				/* If the texture is found within the metadata array, assign it */
				for (i=0; i<ctx->vram.count; i++) {
					if (ctx->vram.metadata[i].z == local_zoom && ctx->vram.metadata[i].x == local_wx && ctx->vram.metadata[i].y == local_wy) {
						tex_id = i;
						ctx->vram.metadata[i].in_use = 1;
						goto assign_texture;
					}
				}

				/* If the texture is not found, find an empty slot */
				for (i=0; i<ctx->vram.count && ctx->vram.metadata[i].in_use; i++);
				assert(i < ctx->vram.count);

				/* Load texture into the empty slot */
				pngdata = tilecache_get(conf, &ctx->cache, &pnglen, local_wx, local_wy, local_zoom);
				if (pngdata) {

					/* Update metadata */
					tex_id = i;
					ctx->vram.metadata[i].in_use = 1;
					ctx->vram.metadata[i].x = local_wx;
					ctx->vram.metadata[i].y = local_wy;
					ctx->vram.metadata[i].z = local_zoom;

					imgdata = stbi_load_from_memory(pngdata, pnglen, &img_w, &img_h, &img_n, 3);
					log_debug("Uploading new texture: (%d,%d,%d) -> %d", local_wx, local_wy, local_zoom, tex_id);

					glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
							0, 0, tex_id,
							MAP_TILE_WIDTH, MAP_TILE_HEIGHT, 1,
							GL_RGB, GL_UNSIGNED_BYTE,
							imgdata);

					free((void*)pngdata);
					free(imgdata);
					goto assign_texture;
				}


				log_debug("Texture not available, trying lower zoom");

				/* If the texture is still not found, check if a lower zoom level one
				 * can be used instead */
				texuv_divisor *= 2;
				texuv_offset_u = (world_x % texuv_divisor) / (float)texuv_divisor;
				texuv_offset_v = (world_y % texuv_divisor) / (float)texuv_divisor;

				local_wx /= 2;
				local_wy /= 2;
			}

assign_texture:
			vertex_start = 4 * (x * y_count + y);
			for (i = 0; i < 4; i++) {
				vertices[vertex_start + i].tex_id = tex_id;
				vertices[vertex_start + i].tex_u = texuv_offset_u + (i / 2) / (float)texuv_divisor;
				vertices[vertex_start + i].tex_v = texuv_offset_v + (i % 2) / (float)texuv_divisor;
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}


static void
resize_texture(GLuint *texture, int count)
{
	/* Build a new texture array */
	if (*texture) glDeleteTextures(1, texture);
	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, *texture);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB,
			MAP_TILE_WIDTH, MAP_TILE_HEIGHT, count,
			0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}



static int
mipmap(float zoom)
{
	return MAX(0, MIN(MAX_ZOOM_LEVEL, floorf(zoom)));
}
/* }}} */
