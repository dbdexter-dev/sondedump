#include <assert.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stddef.h>
#include "decode.h"
#include "gl_skewt.h"
#include "shaders/shaders.h"
#include "style.h"
#include "utils.h"

#define VCOUNT 16

typedef struct {
	float t;
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
	if (ctx->cp_vbo) glDeleteBuffers(1, &ctx->cp_vbo);
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
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->cp_vbo);

	/* For each color/thickness combination */
	for (i=0; i < SYMSIZE(_binary_skewt_index_bin) / sizeof(BezierMetadata); i++) {
		/* Get metadata */
		BezierMetadata *metadata = ((BezierMetadata*)_binary_skewt_index_bin_start) + i;

		thickness = 2.0 / zoom;

		/* Upload line info */
		glUniform4fv(ctx->u4f_color, 1, metadata->color);
		glUniform1f(ctx->u1f_thickness, thickness);
		glUniform1f(ctx->u1f_frag_thickness, thickness / 2.0 * zoom);

		/* Upload curves to GPU mem */
		glBufferData(GL_ARRAY_BUFFER, metadata->len, _binary_skewt_bin_start + metadata->offset, GL_STREAM_DRAW);
		/* Render (could be drawArrays for now but maybe not in the future, and
		 * the memory requirements for the index buffer are tiny even at high
		 * tessellation) */
		glDrawElementsInstanced(GL_TRIANGLE_STRIP, VCOUNT + 2, GL_UNSIGNED_INT, 0, metadata->len / sizeof(Bezier));
	}
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
#define Y_INTO_X_COEFF 0.92587511

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
	Vertex vertices[VCOUNT + 2];
	unsigned int ibo[LEN(vertices)];

	GLint status, attrib_t, attrib_p[4];
	int i;

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
	attrib_t = glGetAttribLocation(ctx->chart_program, "in_t");
	attrib_p[0] = glGetAttribLocation(ctx->chart_program, "in_p0");
	attrib_p[1] = glGetAttribLocation(ctx->chart_program, "in_p1");
	attrib_p[2] = glGetAttribLocation(ctx->chart_program, "in_p2");
	attrib_p[3] = glGetAttribLocation(ctx->chart_program, "in_p3");


	/* Allocate buffers */
	glGenVertexArrays(1, &ctx->vao);
	glBindVertexArray(ctx->vao);
	glGenBuffers(1, &ctx->vbo);
	glGenBuffers(1, &ctx->ibo);
	glGenBuffers(1, &ctx->cp_vbo);

	/* Generate vertices for Bezier tessellation (2x each line point, will be
	 * expanded into a thick line by the vertex shader). Need +- 1.0 so that all
	 * even t's are strictly positive and all odd t's are strictly negative */
	for (i=0; i < (int)LEN(vertices); i+=2) {
		vertices[i].t = (float)i/(LEN(vertices) - 2) + 1.0;
		vertices[i+1].t = -(float)i/(LEN(vertices) - 2) - 1.0;
	}

	/* Generate indices */
	for (size_t i=0; i<LEN(ibo); i++) {
		ibo[i] = i;
	}

	/* Bind and upload the timeseries vbo and ibo */
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
	glEnableVertexAttribArray(attrib_t);
	glVertexAttribPointer(attrib_t, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, t));

	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ibo), ibo, GL_STATIC_DRAW);

	/* Describe the control points vbo */
	glBindBuffer(GL_ARRAY_BUFFER, ctx->cp_vbo);
	for (i=0; i < (int)LEN(attrib_p); i++) {
		glEnableVertexAttribArray(attrib_p[i]);
		glVertexAttribPointer(attrib_p[i], 2, GL_FLOAT, GL_FALSE, sizeof(Bezier), (void*)offsetof(Bezier, cp[i]));

		glVertexAttribDivisor(attrib_p[i], 1);
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
/* }}} */
