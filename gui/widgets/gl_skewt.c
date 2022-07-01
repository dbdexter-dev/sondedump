#include <assert.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#include "decode.h"
#include "gl_skewt.h"
#include "shaders/shaders.h"
#include "style.h"
#include "utils.h"

#define BEZIER_VERTEX_COUNT(steps) (2 * ((steps) + 1))

#define VCOUNT 16
#define LINE_THICKNESS 1

typedef struct {
	float position[2];
	float normal[2];
} Vertex;

typedef struct {
	float cp[4][2];
} Bezier;

typedef struct {
	uint32_t offset;
	uint32_t len;
	float color[4];
	float thickness;
} BezierMetadata;

extern const char _binary_skewt_bin_start[];
extern const char _binary_skewt_bin_end[];
extern const char _binary_skewt_index_bin_start[];
extern const char _binary_skewt_index_bin_end[];

static void chart_opengl_init(GLSkewT *ctx);
static void data_opengl_init(GLSkewT *ctx);
static void bezier_tessellate(Vertex *dst, int steps, const float ctrl_points[4][2]);
static void bezier(float dst[2], float t, const float ctrl_points[4][2]);
static void bezier_deriv(float dst[2], float t, const float ctrl_points[4][2]);

void
gl_skewt_init(GLSkewT *ctx)
{
	chart_opengl_init(ctx);
	data_opengl_init(ctx);
}

void
gl_skewt_deinit(GLSkewT *ctx)
{
	unsigned int i;
	if (ctx->vao) glDeleteVertexArrays(1, &ctx->vao);
	if (ctx->vbo) glDeleteBuffers(1, &ctx->vbo);
	for (i=0; i<LEN(ctx->ibo); i++) {
		if (ctx->ibo[i]) glDeleteBuffers(1, &ctx->ibo[i]);
	}
	if (ctx->chart_vert_shader) glDeleteProgram(ctx->chart_vert_shader);
	if (ctx->chart_frag_shader) glDeleteProgram(ctx->chart_frag_shader);
	if (ctx->chart_program) glDeleteProgram(ctx->chart_program);
}

void
gl_skewt_vector(GLSkewT *ctx, float width, float height)
{
	const float zoom = ctx->zoom;
	const float x_center = ctx->center_x;
	const float y_center = ctx->center_y;
	const int data_count = get_data_count();
	size_t i;
	float thickness;
	float temperature_color[] = {1.0, 0.0, 0.0, 1.0};
	float dewpt_color[] = {0.0, 0.0, 1.0, 1.0};


	GLfloat proj[4][4] = {
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, -1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	};

	proj[0][0] *= 2.0f / (float)width * zoom;
	proj[1][1] *= 2.0f / (float)height * zoom;
	proj[3][0] = proj[0][0] * (-x_center);
	proj[3][1] = proj[1][1] * (-y_center);

	/* Draw background {{{ */
	glUseProgram(ctx->chart_program);
	glBindVertexArray(ctx->vao);
	glEnable(GL_BLEND);
	glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);

	/* For each color/thickness combination */
	for (i=0; i < SYMSIZE(_binary_skewt_index_bin) / sizeof(BezierMetadata); i++) {
		/* Get metadata */
		BezierMetadata *metadata = ((BezierMetadata*)_binary_skewt_index_bin_start) + i;
		unsigned int vertex_count = BEZIER_VERTEX_COUNT(VCOUNT) * (metadata->len / sizeof(Bezier));
		unsigned int index_count = vertex_count + (metadata->len / sizeof(Bezier));

		thickness = 2.0 * LINE_THICKNESS / zoom;

		/* Upload line group metadata  */
		glUniform4fv(ctx->u4f_color, 1, metadata->color);
		glUniform1f(ctx->u1f_thickness, thickness);
		glUniform1f(ctx->u1f_frag_thickness, thickness / 2.0 * zoom);

		/* Render line group */
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ibo[i]);
		glDrawElements(GL_TRIANGLE_STRIP, index_count, GL_UNSIGNED_INT, 0);
	}
	glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
	glDisable(GL_BLEND);
	/* }}} */
	/* Draw data {{{ */
	glUseProgram(ctx->data_program);
	glBindVertexArray(ctx->data_vao);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->data_vbo);
	glBufferData(GL_ARRAY_BUFFER, data_count * sizeof(GeoPoint), get_track_data(), GL_STREAM_DRAW);

#define BG_HEIGHT_WIDTH_RATIO 0.839583
#define TEMP_OFFSET (-122.5)
#define TEMP_SCALE ((-24.5 - TEMP_OFFSET))
#define PRESS_OFFSET (logf(100))
#define PRESS_SCALE ((logf(1050) - PRESS_OFFSET) / BG_HEIGHT_WIDTH_RATIO)
#define Y_INTO_X_COEFF (tanf(42.77*M_PI/180))

	float a = proj[0][0];
	float b = proj[1][1];
	float c = proj[3][0];
	float d = proj[3][1];

	proj[0][0] = a / TEMP_SCALE;
	proj[1][0] = -a * Y_INTO_X_COEFF / PRESS_SCALE;
	proj[3][0] = c - a * TEMP_OFFSET / TEMP_SCALE + a * Y_INTO_X_COEFF * PRESS_OFFSET / PRESS_SCALE;
	proj[1][1] = b / PRESS_SCALE;
	proj[3][1] = -b * PRESS_OFFSET / PRESS_SCALE + d;
	proj[2][2] = 1;
	proj[3][3] = 1;

	/* Upload transformation matrix */
	glUniformMatrix4fv(ctx->u4m_data_proj, 1, GL_FALSE, (GLfloat*)proj);

	/* Draw dew point */
	glUniform4fv(ctx->u4f_data_color, 1, dewpt_color);
	glVertexAttribPointer(ctx->attrib_data_temp, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, dewpt));
	glDrawArrays(GL_POINTS, 0, data_count);

	/* Draw air temperature */
	glUniform4fv(ctx->u4f_data_color, 1, temperature_color);
	glVertexAttribPointer(ctx->attrib_data_temp, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, temp));
	glDrawArrays(GL_POINTS, 0, data_count);

	/* }}} */
}

/* Static functions {{{ */
static void
chart_opengl_init(GLSkewT *ctx)
{
	Vertex *vertices;
	Bezier *bez;
	BezierMetadata *metadata;
	unsigned int *indices;

	GLint status, attrib_pos, attrib_normal;
	unsigned int i, j, k, vertex_count, vertex_offset;

	const GLchar *vertex_shader = _binary_bezierproj_vert_start;
	const int vertex_shader_len = SYMSIZE(_binary_bezierproj_vert);
	const GLchar *fragment_shader = _binary_feathercolor_frag_start;
	const int fragment_shader_len = SYMSIZE(_binary_feathercolor_frag);

	ctx->chart_program = glCreateProgram();
	ctx->chart_vert_shader = glCreateShader(GL_VERTEX_SHADER);
	ctx->chart_frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(ctx->chart_vert_shader, 1, &vertex_shader, &vertex_shader_len);
	glShaderSource(ctx->chart_frag_shader, 1, &fragment_shader, &fragment_shader_len);

	/* Compile and link shaders */
	glCompileShader(ctx->chart_vert_shader);
	glGetShaderiv(ctx->chart_vert_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glCompileShader(ctx->chart_frag_shader);
	glGetShaderiv(ctx->chart_frag_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glAttachShader(ctx->chart_program, ctx->chart_vert_shader);
	glAttachShader(ctx->chart_program, ctx->chart_frag_shader);
	glLinkProgram(ctx->chart_program);

	glGetProgramiv(ctx->chart_program, GL_LINK_STATUS, &status);
	assert(status == GL_TRUE);

	/* Find uniforms + attributes */
	ctx->u4m_proj = glGetUniformLocation(ctx->chart_program, "u_proj_mtx");
	ctx->u4f_color = glGetUniformLocation(ctx->chart_program, "u_color");
	ctx->u1f_thickness = glGetUniformLocation(ctx->chart_program, "u_thickness");
	ctx->u1f_frag_thickness = glGetUniformLocation(ctx->chart_program, "u_aa_thickness");
	attrib_pos = glGetAttribLocation(ctx->chart_program, "in_position");
	attrib_normal = glGetAttribLocation(ctx->chart_program, "in_normal");

	memset(ctx->ibo, 0, sizeof(ctx->vbo));

	/* Allocate buffers */
	glGenVertexArrays(1, &ctx->vao);
	glBindVertexArray(ctx->vao);
	glGenBuffers(1, &ctx->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);

	vertex_count = 0;
	for (i=0; i < SYMSIZE(_binary_skewt_index_bin) / sizeof(BezierMetadata); i++) {
		metadata = ((BezierMetadata*)_binary_skewt_index_bin_start) + i;
		vertex_count += BEZIER_VERTEX_COUNT(VCOUNT) * (metadata->len / sizeof(Bezier));
	}

	/* Allocate GPU mem for all vertices */
	glBufferData(GL_ARRAY_BUFFER, sizeof(*vertices) * vertex_count, NULL, GL_STATIC_DRAW);

	/* Describe vertex geometry */
	glEnableVertexAttribArray(attrib_pos);
	glVertexAttribPointer(attrib_pos, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glEnableVertexAttribArray(attrib_normal);
	glVertexAttribPointer(attrib_normal, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

	/* Tessellate curves */
	vertex_offset = 0;
	unsigned int index_offset = 0;
	for (i=0; i < SYMSIZE(_binary_skewt_index_bin) / sizeof(BezierMetadata); i++) {
		metadata = ((BezierMetadata*)_binary_skewt_index_bin_start) + i;
		vertex_count = BEZIER_VERTEX_COUNT(VCOUNT) * (metadata->len / sizeof(Bezier));
		vertices = malloc(sizeof(*vertices) * vertex_count);

        /* One index per vertex plus one at the end of the curve to restart */
		indices = malloc(sizeof(*indices) * (vertex_count + metadata->len / sizeof(Bezier)));

		/* Tessellate bezier segments */
		for (j=0; j<metadata->len / sizeof(Bezier); j++) {
			/* Retrieve control points */
			bez = ((Bezier*)(_binary_skewt_bin_start + metadata->offset)) + j;

			/* Tessellate bezier */
			bezier_tessellate(vertices + j*BEZIER_VERTEX_COUNT(VCOUNT), VCOUNT, bez->cp);

			/* Generate indices */
			for (k=0; k<BEZIER_VERTEX_COUNT(VCOUNT); k++) {
				indices[j * (BEZIER_VERTEX_COUNT(VCOUNT) + 1) + k] = index_offset++;
			}
			/* Restart index */
			indices[j * (BEZIER_VERTEX_COUNT(VCOUNT) + 1) + k] = 0xFFFFFFFF;
		}

		/* Upload results to the GPU */
		glBufferSubData(GL_ARRAY_BUFFER, vertex_offset, sizeof(*vertices) * vertex_count, vertices);
		vertex_offset += sizeof(*vertices) * vertex_count;

		glGenBuffers(1, &ctx->ibo[i]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ibo[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(*indices) * (vertex_count + metadata->len/sizeof(Bezier)), indices, GL_STATIC_DRAW);

		free(vertices);
		free(indices);
	}

}

static void
data_opengl_init(GLSkewT *ctx)
{
	GLuint attrib_alt;
	GLint status;

	const GLchar *vertex_shader = _binary_skewtproj_vert_start;
	const int vertex_shader_len = SYMSIZE(_binary_skewtproj_vert);
	const GLchar *fragment_shader = _binary_simplecolor_frag_start;
	const int fragment_shader_len = SYMSIZE(_binary_simplecolor_frag);

	/* Program + shaders */
	ctx->data_program = glCreateProgram();
	ctx->data_vert_shader = glCreateShader(GL_VERTEX_SHADER);
	ctx->data_frag_shader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(ctx->data_vert_shader, 1, &vertex_shader, &vertex_shader_len);
	glShaderSource(ctx->data_frag_shader, 1, &fragment_shader, &fragment_shader_len);

	glCompileShader(ctx->data_vert_shader);
	glGetShaderiv(ctx->data_vert_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glCompileShader(ctx->data_frag_shader);
	glGetShaderiv(ctx->data_frag_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glAttachShader(ctx->data_program, ctx->data_vert_shader);
	glAttachShader(ctx->data_program, ctx->data_frag_shader);
	glLinkProgram(ctx->data_program);

	glGetProgramiv(ctx->data_program, GL_LINK_STATUS, &status);
	assert(status == GL_TRUE);

	/* Uniforms + attributes */
	ctx->u4m_data_proj = glGetUniformLocation(ctx->data_program, "proj_mtx");
	ctx->u4f_data_color = glGetUniformLocation(ctx->data_program, "color");

	ctx->attrib_data_temp = glGetAttribLocation(ctx->data_program, "in_temp");
	attrib_alt = glGetAttribLocation(ctx->data_program, "in_altitude");

	glGenVertexArrays(1, &ctx->data_vao);
	glBindVertexArray(ctx->data_vao);
	glGenBuffers(1, &ctx->data_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->data_vbo);

	glEnableVertexAttribArray(ctx->attrib_data_temp);
	glEnableVertexAttribArray(attrib_alt);

	glVertexAttribPointer(attrib_alt, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, pressure));
}

static void
bezier_tessellate(Vertex *dst, int steps, const float ctrl_points[4][2])
{
	float deriv[2];
	float norm;
	float t;
	int i;

	for (i=0; i<=steps; i++) {
		t = (float)i / steps;
		bezier(dst->position, t, ctrl_points);
		bezier_deriv(deriv, t, ctrl_points);

		norm = sqrtf(deriv[0]*deriv[0] + deriv[1]*deriv[1]);

		dst->normal[0] = -deriv[1] / norm;
		dst->normal[1] = deriv[0] / norm;

		dst++;

		/* Companion vertex has the same everything but opposite normal */
		memcpy(dst->position, (dst-1)->position, sizeof(dst->position));
		dst->normal[0] = -(dst-1)->normal[0];
		dst->normal[1] = -(dst-1)->normal[1];

		dst++;
	}
}

static void
bezier(float dst[2], float t, const float ctrl_points[4][2])
{
	const float t2 = t * t;
	const float t3 = t * t2;
	int i;


	for (i=0; i<2; i++) {
		dst[i] = ctrl_points[0][i]
		       + 3.0 * t * (ctrl_points[1][i] - ctrl_points[0][i])
		       + 3.0 * t2 * (ctrl_points[0][i] - 2.0 * ctrl_points[1][i] + ctrl_points[2][i])
		       + t3 * (ctrl_points[3][i] - 3.0 * ctrl_points[2][i] + 3.0 * ctrl_points[1][i] - ctrl_points[0][i]);
	}
}

static void
bezier_deriv(float dst[2], float t, const float ctrl_points[4][2])
{
	const float t2 = t * t;
	int i;

	for (i=0; i<2; i++) {
		float a = ctrl_points[3][i] - 3.0 * ctrl_points[2][i] + 3.0 * ctrl_points[1][i] - ctrl_points[0][i];
		float b = 3.0 * (ctrl_points[0][i] - 2.0 * ctrl_points[1][i] + ctrl_points[2][i]);
		float c = 3.0 * (ctrl_points[1][i] - ctrl_points[0][i]);

		dst[i] = 3.0 * t2 * a
		       + 2.0 * t * b
		       + c;
	}
}
/* }}} */
