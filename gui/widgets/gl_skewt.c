#include <assert.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stddef.h>
#include "decode.h"
#include "gl_skewt.h"
#include "style.h"
#include "utils.h"
#include "shaders/shaders.h"

#define VCOUNT 40

typedef struct {
	float t;
} Vertex;

static void chart_opengl_init(GLSkewT *ctx);
static void data_opengl_init(GLSkewT *ctx);

void
gl_skewt_init(GLSkewT *ctx)
{
	chart_opengl_init(ctx);
	data_opengl_init(ctx);
}

void
gl_skewt_deinit(GLSkewT *ctx)
{
	if (ctx->vao) glDeleteVertexArrays(1, &ctx->vao);
	if (ctx->vbo) glDeleteBuffers(1, &ctx->vbo);
	if (ctx->ibo) glDeleteBuffers(1, &ctx->ibo);
	if (ctx->chart_vert_shader) glDeleteProgram(ctx->chart_vert_shader);
	if (ctx->chart_frag_shader) glDeleteProgram(ctx->chart_frag_shader);
	if (ctx->chart_program) glDeleteProgram(ctx->chart_program);
}

void
gl_skewt_raster(GLSkewT *ctx, float width, float height, float x_center, float y_center, float zoom)
{
	const float chart_color[] = STYLE_ACCENT_1_NORMALIZED;

	GLfloat proj[4][4] = {
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	};
	GLfloat control_points[][2] = {{100, 100}, {200, 200}, {200, 200}, {400, 100}};
	GLfloat control_points_2[][2] = {{400, 100}, {-100, 200}, {-200, -800}, {0, 0}};
	GLfloat control_points_3[][2] = {{0, 0}, {0, 1000}, {-200, -800}, {500, 500}};

	proj[0][0] *= 2.0f / (float)width * zoom;
	proj[1][1] *= 2.0f / (float)height * zoom;
	proj[3][0] = proj[0][0] * (-x_center);
	proj[3][1] = proj[1][1] * (-y_center);

	/* Draw background */
	glUseProgram(ctx->chart_program);
	glBindVertexArray(ctx->vao);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUniform4fv(ctx->u4f_color, 1, chart_color);
	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);
	glUniform1f(ctx->u1f_thickness, 2.0);

	glUniform2fv(ctx->u2f_control_points, 4, (GLfloat*)control_points);
	glDrawElements(GL_TRIANGLES, 6 * VCOUNT, GL_UNSIGNED_INT, 0);

	glUniform2fv(ctx->u2f_control_points, 4, (GLfloat*)control_points_2);
	glDrawElements(GL_TRIANGLES, 6 * VCOUNT, GL_UNSIGNED_INT, 0);

	glUniform2fv(ctx->u2f_control_points, 4, (GLfloat*)control_points_3);
	glDrawElements(GL_TRIANGLES, 6 * VCOUNT, GL_UNSIGNED_INT, 0);
	glDisable(GL_BLEND);

#if 0
	/* Draw data */
	glUseProgram(ctx->data_program);
	glBindVertexArray(ctx->data_vao);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->data_vbo);
#endif
}

/* Static functions {{{ */
static void
chart_opengl_init(GLSkewT *ctx)
{
	Vertex vertices[2 * VCOUNT + 2];
	unsigned int ibo[3 * LEN(vertices)];

	/* Test pattern TODO keep this but make it nicer */
	for (size_t i=0; i<LEN(vertices); i+=2) {
		vertices[i].t = (float)i/(LEN(vertices) - 2);
		vertices[i+1].t = -(float)i/(LEN(vertices) - 2);
	}
	int d = 0;
	for (size_t i=0; i<LEN(ibo)-6; i+=6) {
		ibo[i] = d+0;
		ibo[i+1] = d+2;
		ibo[i+2] = d+1;

		ibo[i+3] = d+2;
		ibo[i+4] = d+1;
		ibo[i+5] = d+3;

		d += 2;
	}
	vertices[0].t = 1e-20;
	vertices[1].t = -1e-20;

	GLint status, attrib_t;

	const GLchar *vertex_shader = _binary_bezierproj_vert_start;
	const int vertex_shader_len = SYMSIZE(_binary_bezierproj_vert);
	const GLchar *fragment_shader = _binary_feathercolor_frag_start;
	const int fragment_shader_len = SYMSIZE(_binary_feathercolor_frag);

	ctx->chart_program = glCreateProgram();
	ctx->chart_vert_shader = glCreateShader(GL_VERTEX_SHADER);
	ctx->chart_frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(ctx->chart_vert_shader, 1, &vertex_shader, &vertex_shader_len);
	glShaderSource(ctx->chart_frag_shader, 1, &fragment_shader, &fragment_shader_len);

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

	ctx->u4m_proj = glGetUniformLocation(ctx->chart_program, "proj_mtx");
	ctx->u4f_color = glGetUniformLocation(ctx->chart_program, "color");
	ctx->u2f_control_points = glGetUniformLocation(ctx->chart_program, "control_points");
	ctx->u1f_thickness = glGetUniformLocation(ctx->chart_program, "thickness");
	attrib_t = glGetAttribLocation(ctx->chart_program, "in_t");

	glGenVertexArrays(1, &ctx->vao);
	glBindVertexArray(ctx->vao);
	glGenBuffers(1, &ctx->vbo);
	glGenBuffers(1, &ctx->ibo);

	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
	glEnableVertexAttribArray(attrib_t);
	glVertexAttribPointer(attrib_t, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, t));

	glBufferData(GL_ARRAY_BUFFER, LEN(vertices) * sizeof(*vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, LEN(ibo) * sizeof(*ibo), ibo, GL_STATIC_DRAW);
}

static void
data_opengl_init(GLSkewT *ctx)
{
}
/* }}} */
