#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>  /* TODO Remove */
#include <GLES3/gl3.h>
#include "decode.h"
#include "gl_osm.h"
#include "stb_image/stb_image.h"
#include "utils.h"

#define MODLT(x, y, mod) (((y) - (x) - 1 + (mod)) % (mod) < ((mod) / 2))
#define MODGE(x, y, mod) (((x) - (y) + (mod)) % (mod) <= ((mod) / 2))

#ifdef __APPLE__
  #define SHADER_VERSION "#version 150\n"
#else
  #define SHADER_VERSION "#version 300 es\n"
#endif

typedef struct {
	float x, y;
	int tex;
} Vertex;

static void map_opengl_init(GLOpenStreetMap *map);
static void track_opengl_init(GLOpenStreetMap *map);
static void build_quads(int x_count, int y_count, Vertex *vertices, unsigned int *indices);
static void resize_texture(GLuint *texture, int count);
static void refresh_textures(GLOpenStreetMap *map, Vertex *vertices, int x_start, int y_start, int x_count, int y_count, int zoom);
static int mipmap(float zoom);


void
gl_openstreetmap_init(GLOpenStreetMap *map)
{
	/* Background map program, buffers, uniforms... */
	map_opengl_init(map);

	/* Ground track program, buffers, uniforms... */
	track_opengl_init(map);

	map->textures = NULL;
	map->tex_array = 0;
	map->texture_count = 0;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void
gl_openstreetmap_deinit(GLOpenStreetMap *map)
{
	if (map->tex_vert_shader) glDeleteProgram(map->tex_vert_shader);
	if (map->tex_frag_shader) glDeleteProgram(map->tex_frag_shader);
	if (map->texture_program) glDeleteProgram(map->texture_program);
	if (map->track_vert_shader) glDeleteProgram(map->track_vert_shader);
	if (map->track_frag_shader) glDeleteProgram(map->track_frag_shader);
	if (map->track_program) glDeleteProgram(map->track_program);
	if (map->vbo) glDeleteBuffers(1, &map->vbo);
	if (map->ibo) glDeleteBuffers(1, &map->ibo);
	if (map->vao) glDeleteVertexArrays(1, &map->vao);
	if (map->tex_array) glDeleteTextures(1, &map->tex_array);
	if (map->track_vao) glDeleteTextures(1, &map->track_vao);
	if (map->track_vbo) glDeleteTextures(1, &map->track_vbo);
	free(map->textures);
	map->texture_count = 0;
}

void
gl_openstreetmap_raster(GLOpenStreetMap *map, int width, int height, float x_center, float y_center, float zoom)
{
	/* Enough to fill the screen plus a 1x border all around for safety */
	const int x_size = 1 << mipmap(zoom);
	const int y_size = 1 << mipmap(zoom);
	const int x_count = ceilf((float)width / MAP_TILE_WIDTH * TILE_MIN_ZOOM) + 2;
	const int y_count = ceilf((float)height / MAP_TILE_HEIGHT * TILE_MIN_ZOOM) + 2;
	const int count = x_count * y_count;
	const float digital_zoom = powf(2, zoom - mipmap(zoom));
	Vertex *vertices;
	unsigned int *indices;
	int x_start, y_start, x_end, y_end;
	int i;

	GLfloat proj[4][4] = {
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f,-1.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	};

	x_start = (int)roundf(x_center - x_count/2);
	y_start = (int)roundf(y_center - y_count/2);
	x_end = x_start + x_count;
	y_end = y_start + y_count;

	/**
	 * Projection transform:
	 * - Pan so that (lat,lon) is at (0,0)
	 * - Scale so that 1 texture pixel = 1 viewport pixel
	 * - Scale by the delta between zoom and mipmap zoom
	 * - Scale by width/height to go from pixel coords to viewport coords
	 */
	proj[0][0] *= 2.0f * MAP_TILE_WIDTH / (float)width * digital_zoom;
	proj[1][1] *= 2.0f * -MAP_TILE_HEIGHT / (float)height * digital_zoom;
	proj[3][0] = proj[0][0] * (x_start - x_center);
	proj[3][1] = proj[1][1] * (y_start - y_center);

	/* Project out-of-bounds tiles back in bounds */
	x_start = (x_start + x_size) % x_size;
	y_start = (y_start + y_size) % y_size;

	/* Setup quads */
	vertices = malloc(sizeof(*vertices) * 4 * count);
	indices = malloc(sizeof(*indices) * 6 * count);
	build_quads(x_count, y_count, vertices, indices);


	/* Resize array to fit all textures */
	if (map->texture_count < count) {
		map->textures = reallocarray(map->textures, count, sizeof(Texture));

		/* Have to start from scratch since the texture 2d array will be deleted */
		for (i=0; i<count; i++) {
			map->textures[i].in_use = 0;
		}

#ifndef NDEBUG
		printf("Texture count: %d -> %d\n", map->texture_count, count);
#endif
		map->texture_count = count;

		/* Resize the actual texture array */
		resize_texture(&map->tex_array, count);
	}

	/* Mark textures that are no longer in use as replaceable (with wraparounds) */
	for (i=0; i<map->texture_count; i++) {
		if (MODLT(map->textures[i].x, x_start, x_size)
		 || MODLT(map->textures[i].y, y_start, y_size)
		 || MODGE(map->textures[i].x, x_end, x_size)
		 || MODGE(map->textures[i].y, y_end, y_size)) {
			map->textures[i].in_use = 0;
		}
	}

	/* Link texture indices to each quad */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, map->tex_array);
	refresh_textures(map, vertices, x_start, y_start, x_count, y_count, mipmap(zoom));
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	/* Setup program state */
	glUseProgram(map->texture_program);
	glBindVertexArray(map->vao);
	glBindBuffer(GL_ARRAY_BUFFER, map->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, map->ibo);

	/* Update quads */
	glBufferData(GL_ARRAY_BUFFER, 4 * count * sizeof(*vertices), vertices, GL_STATIC_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * count * sizeof(*indices), indices, GL_STATIC_DRAW);

	/* Bind correct texture to shaders */
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, map->tex_array);
	glUniform1i(map->u1i_texture, 0);

	/* Setup model-view-projection */
	glUniformMatrix4fv(map->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);

	/* Draw tiles (ibo, vao & vbo already bound) */
	glDrawElements(GL_TRIANGLES, 6 * count, GL_UNSIGNED_INT, 0);


	/* Swap to the track program */
	glUseProgram(map->track_program);
	glBindVertexArray(map->track_vao);
	glBindBuffer(GL_ARRAY_BUFFER, map->track_vbo);
	glBufferData(GL_ARRAY_BUFFER, get_data_count() * sizeof(GeoPoint), get_track_data(), GL_STREAM_DRAW);

	proj[3][0] = proj[0][0] * -x_center;
	proj[3][1] = proj[1][1] * -y_center;

	glUniformMatrix4fv(map->u4m_track_proj, 1, GL_FALSE, (GLfloat*)proj);
	glUniform4f(map->u4f_track_color, 1.0, 0.0, 0.0, 1.0);
	glUniform1f(map->u1f_zoom, 1 << mipmap(zoom));

	glDrawArrays(GL_POINTS, 0, get_data_count());

	/* Cleanup */
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    //glDisable(GL_BLEND);

	free(vertices);
	free(indices);
}

/* Static functions {{{ */
static void
map_opengl_init(GLOpenStreetMap *map)
{
	GLuint attrib_pos, attrib_tex;
	GLint status;

	static const GLchar *vertex_shader =
		SHADER_VERSION
        "precision mediump float;\n"

		"uniform mat4 ProjMtx;\n"

		"in vec2 Position;\n"
		"in int TextureID;\n"

		"out vec2 TexUV_raw;\n"
		"flat out int TexID;\n"

		"void main() {\n"
		"   TexID = TextureID;\n"
		"   TexUV_raw = Position.xy;\n"
		"   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
		//"   gl_Position = vec4(Position.xy, 0, 1);\n"
		"}";
	static const GLchar *fragment_shader =
		SHADER_VERSION
        "precision mediump float;\n"
        "precision mediump sampler2DArray;\n"

		"uniform sampler2DArray Texture;\n"

		"in vec2 TexUV_raw;\n"
		"flat in int TexID;\n"

		"out vec4 Out_Color;\n"

		"void main() {\n"
		"   vec2 TexUV = fract(TexUV_raw);\n"
		"   float z = float(TexID);\n"
		"   Out_Color = texture(Texture, vec3(TexUV, z));\n"
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
	map->u1i_texture = glGetUniformLocation(map->texture_program, "Texture");
	map->u4m_proj = glGetUniformLocation(map->texture_program, "ProjMtx");

	attrib_pos = glGetAttribLocation(map->texture_program, "Position");
	attrib_tex = glGetAttribLocation(map->texture_program, "TextureID");

	/* Buffers, arrays, and layouts */
	glGenVertexArrays(1, &map->vao);
	glBindVertexArray(map->vao);

	glGenBuffers(1, &map->vbo);
	glGenBuffers(1, &map->ibo);

	glBindBuffer(GL_ARRAY_BUFFER, map->vbo);

	glEnableVertexAttribArray(attrib_pos);
	glVertexAttribPointer(attrib_pos, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
	glEnableVertexAttribArray(attrib_tex);
	glVertexAttribIPointer(attrib_tex, 1, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, tex));
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

	//glEnable(0x8642);

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
resize_texture(GLuint *texture, int count)
{
	/* Build a new texture array */
	if (*texture) glDeleteTextures(1, texture);
	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, *texture);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB,
			MAP_TILE_WIDTH, MAP_TILE_HEIGHT, count,
			0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

static void
refresh_textures(GLOpenStreetMap *map, Vertex *vertices, int x_start, int y_start, int x_count, int y_count, int zoom)
{
	const int x_size = 1 << zoom;
	const int y_size = 1 << zoom;
	FILE *fd;
	int i, j, x, y, actual_x, actual_y;
	int idx;
	void *image;
	char path[256];
	size_t len;
	void *pngdata;
	int width, height, n;

	for (x = 0; x < x_count; x++) {
		actual_x = (x_start + x) % x_size;
		for (y = 0; y < y_count; y++) {
			actual_y = (y_start + y) % y_size;
			for (i=0; i<map->texture_count; i++) {
				if (map->textures[i].x == actual_x && map->textures[i].y == actual_y) {
					/* Texture already loaded: exit early */
					break;
				}
			}

			if (i == map->texture_count) {
				/* Texture is not in memory yet: find an empty slot */
				for (i=0; i<map->texture_count && map->textures[i].in_use; i++)
					;

				map->textures[i].x = actual_x;
				map->textures[i].y = actual_y;
				map->textures[i].in_use = 1;

				/* Load new texture from file */
				sprintf(path, "/home/dbdexter/Projects/magellan/out/%d/%d/%d.png", zoom, actual_x, actual_y);
				fd = fopen(path, "rb");
				fseek(fd, 0, SEEK_END);
				len = ftell(fd);
				fseek(fd, 0, SEEK_SET);
				pngdata = malloc(len);
				fread(pngdata, len, 1, fd);
				fclose(fd);

				image = stbi_load_from_memory(pngdata, len, &width, &height, &n, 3);
				free(pngdata);

				/* Copy texture into 2d tex array at offset i (already bound) */
				glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
						0, 0, i,
						MAP_TILE_WIDTH, MAP_TILE_HEIGHT, 1,
						GL_RGB, GL_UNSIGNED_BYTE,
						image);

				free(image);

#ifndef NDEBUG
				printf("Loaded texture (%d,%d) -> [%d]\n", actual_x, actual_y, i);
#endif

			}

			/* Associate quad to index */
			for (j=0; j<4; j++) {
				idx = j + y * 4 + x * 4 * y_count;
				vertices[idx].tex = i;
			}
		}
	}
}

static int
mipmap(float zoom)
{
	if (zoom >= 8.0f) return 8;

	return 8;
}

/* }}} */
