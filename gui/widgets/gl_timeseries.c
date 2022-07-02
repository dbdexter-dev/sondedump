#include <assert.h>
#include <stddef.h>
#include "decode.h"
#include "gl_timeseries.h"
#include "shaders/shaders.h"
#include "utils.h"
#include "style.h"

static void timeseries_opengl_init(GLTimeseries *ctx);

void
gl_timeseries_init(GLTimeseries *ctx)
{
	timeseries_opengl_init(ctx);
}


void gl_timeseries_deinit(GLTimeseries *ctx)
{
	if (ctx->vao) glDeleteVertexArrays(1, &ctx->vao);
	if (ctx->vbo) glDeleteBuffers(1, &ctx->vbo);
	if (ctx->vert_shader) glDeleteProgram(ctx->vert_shader);
	if (ctx->frag_shader) glDeleteProgram(ctx->frag_shader);
	if (ctx->program) glDeleteProgram(ctx->program);
}


void
gl_timeseries(GLTimeseries *ctx, const GeoPoint *data, size_t len)
{
	const float temp_bounds[] = {-100, 50+100};
	const float rh_bounds[] = {0, 100-0};
	const float alt_bounds[] = {0, 40000-0};

	const float temperature_color[] = STYLE_ACCENT_1_NORMALIZED;
	const float dewpt_color[] = STYLE_ACCENT_0_NORMALIZED;
	const float alt_color[] = STYLE_ACCENT_3_NORMALIZED;
	const float rh_color[] = STYLE_ACCENT_2_NORMALIZED;
	const int max_id = len > 0 ? MAX(1, data[len-1].id) : 1;
	const int min_id = len > 0 ? data[0].id : 0;
	float p11;

	GLfloat proj[4][4] = {
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	};

	proj[0][0] *= 2.0f / MAX(1, max_id - min_id);
	proj[1][1] *= 2.0f;
	proj[3][0] = -1 - proj[0][0] * min_id;
	p11 = proj[1][1];

	glUseProgram(ctx->program);
	glBindVertexArray(ctx->vao);

	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
	glBufferData(GL_ARRAY_BUFFER, len * sizeof(GeoPoint), data, GL_STREAM_DRAW);

	/* Temperature */
	proj[1][1] = p11 / temp_bounds[1];
	proj[3][1] = -1 + proj[1][1] * (-temp_bounds[0]);
	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);
	glUniform4fv(ctx->u4f_color, 1, temperature_color);
	glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, temp));
	glDrawArrays(GL_POINTS, 0, len);

	/* RH */
	proj[1][1] = p11 / rh_bounds[1];
	proj[3][1] = -1 + proj[1][1] * (-rh_bounds[0]);
	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);
	glUniform4fv(ctx->u4f_color, 1, rh_color);
	glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, rh));
	glDrawArrays(GL_POINTS, 0, len);

	/* Dew point */
	proj[1][1] = p11 / temp_bounds[1];
	proj[3][1] = -1 + proj[1][1] * (-temp_bounds[0]);
	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);
	glUniform4fv(ctx->u4f_color, 1, dewpt_color);
	glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, dewpt));
	glDrawArrays(GL_POINTS, 0, len);

	/* Altitude */
	proj[1][1] = p11 / alt_bounds[1];
	proj[3][1] = -1 + proj[1][1] * (-alt_bounds[0]);
	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);
	glUniform4fv(ctx->u4f_color, 1, alt_color);
	glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, alt));
	glDrawArrays(GL_POINTS, 0, len);

	/* Cleanup */
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

/* Static functions {{{ */
static void
timeseries_opengl_init(GLTimeseries *ctx)
{
	GLint status;

	const GLchar *vertex_shader = _binary_simpleproj_vert_start;
	const int vertex_shader_len = SYMSIZE(_binary_simpleproj_vert);
	const GLchar *fragment_shader = _binary_simplecolor_frag_start;
	const int fragment_shader_len = SYMSIZE(_binary_simplecolor_frag);

	ctx->program = glCreateProgram();
	ctx->vert_shader = glCreateShader(GL_VERTEX_SHADER);
	ctx->frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(ctx->vert_shader, 1, &vertex_shader, &vertex_shader_len);
	glShaderSource(ctx->frag_shader, 1, &fragment_shader, &fragment_shader_len);

	/* Compile and link shaders */
	glCompileShader(ctx->vert_shader);
	glGetShaderiv(ctx->vert_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glCompileShader(ctx->frag_shader);
	glGetShaderiv(ctx->frag_shader, GL_COMPILE_STATUS, &status);
	assert(status == GL_TRUE);

	glAttachShader(ctx->program, ctx->vert_shader);
	glAttachShader(ctx->program, ctx->frag_shader);
	glLinkProgram(ctx->program);

	glGetProgramiv(ctx->program, GL_LINK_STATUS, &status);
	assert(status == GL_TRUE);

	/* Uniforms + attributes */
	ctx->u4m_proj = glGetUniformLocation(ctx->program, "u_proj_mtx");
	ctx->u4f_color = glGetUniformLocation(ctx->program, "u_color");
	ctx->attrib_pos_x = glGetAttribLocation(ctx->program, "in_position_x");
	ctx->attrib_pos_y = glGetAttribLocation(ctx->program, "in_position_y");

	/* Allocate buffers */
	glGenVertexArrays(1, &ctx->vao);
	glBindVertexArray(ctx->vao);
	glGenBuffers(1, &ctx->vbo);

	/* Bind the timeseries vbo */
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
	glEnableVertexAttribArray(ctx->attrib_pos_x);
	glEnableVertexAttribArray(ctx->attrib_pos_y);
	glVertexAttribPointer(ctx->attrib_pos_x, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, id));
}
/* }}} */