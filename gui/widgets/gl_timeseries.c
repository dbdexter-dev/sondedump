#include <assert.h>
#include <math.h>
#include <stddef.h>
#include "libs/glad/glad.h"
#include "log/log.h"
#include "decode.h"
#include "gui/gl_utils.h"
#include "gl_timeseries.h"
#include "shaders/shaders.h"
#include "utils.h"
#include "style.h"

/* Types of data that can be displayed */
enum datatype {
	TEMP,
	RH,
	DEWPT,
	PRESS,
	ALT,
	SPD,
	HDG,
	CLIMB,
};

static void timeseries_opengl_init(GLTimeseries *ctx);

static void gl_timeseries_multi(const enum datatype *datatypes, int types_count, GLTimeseries *ctx, const Config *config, const GeoPoint *data, size_t len, const GeoPoint *maxima, const GeoPoint *minima);
static void gl_timeseries_single(enum datatype datatype, GLTimeseries *ctx, const Config *config, const GeoPoint *data, size_t len, const GeoPoint *maxima, const GeoPoint *minima);

void
gl_timeseries_init(GLTimeseries *ctx)
{
	timeseries_opengl_init(ctx);
}


void gl_timeseries_deinit(GLTimeseries *ctx)
{
	if (ctx->vao) glDeleteVertexArrays(1, &ctx->vao);
	if (ctx->vbo) glDeleteBuffers(1, &ctx->vbo);
	if (ctx->program) glDeleteProgram(ctx->program);
}

void
gl_timeseries_ptu(GLTimeseries *ctx, const Config *config, const GeoPoint *data, size_t len, const GeoPoint *maxima, const GeoPoint *minima)
{
	const enum datatype channels[] = {TEMP, RH, DEWPT, PRESS};

	gl_timeseries_multi(channels, LEN(channels), ctx, config, data, len, maxima, minima);
}

void
gl_timeseries_gps(GLTimeseries *ctx, const Config *config, const GeoPoint *data, size_t len, const GeoPoint *maxima, const GeoPoint *minima)
{
	const enum datatype channels[] = {ALT, SPD, HDG, CLIMB};

	gl_timeseries_multi(channels, LEN(channels), ctx, config, data, len, maxima, minima);
}


/* Static functions {{{ */
static void
gl_timeseries_multi(const enum datatype *datatypes, int types_count, GLTimeseries *ctx, const Config *config, const GeoPoint *data, size_t len, const GeoPoint *maxima, const GeoPoint *minima)
{
	int i;

	/* Select program */
	glUseProgram(ctx->program);
	glBindVertexArray(ctx->vao);

	/* Upload new data */
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
	glBufferData(GL_ARRAY_BUFFER, len * sizeof(GeoPoint), data, GL_STREAM_DRAW);

	/* Draw points for each channel */
	for (i=0; i<types_count; i++) {
		/* Set up uniforms */
		gl_timeseries_single(datatypes[i], ctx, config, data, len, maxima, minima);

		/* Draw */
		glDrawArrays(GL_POINTS, 0, len);
	}

	/* Cleanup */
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

/**
 * Set up uniforms and VAO to draw a specific field inside the geopoint struct
 */
static void
gl_timeseries_single(enum datatype datatype, GLTimeseries *ctx, const Config *config, const GeoPoint *data, size_t len, const GeoPoint *maxima, const GeoPoint *minima)
{
	const float temp_bounds[] = {fmin(minima->temp, minima->dewpt), maxima->temp - fmin(minima->temp, minima->dewpt)};
	const float rh_bounds[] = {minima->rh, maxima->rh - minima->rh};
	const float alt_bounds[] = {minima->alt, maxima->alt - minima->alt};
	const float press_bounds[] = {minima->pressure, maxima->pressure - minima->pressure};
	const float hdg_bounds[] = {minima->hdg, maxima->hdg - minima->hdg};
	const float climb_bounds[] = {minima->climb, maxima->climb - minima->climb};
	const float spd_bounds[] = {minima->spd, maxima->spd - minima->spd};

	const float *color = config->colors.temp;

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


	switch (datatype) {
	case TEMP:
		proj[1][1] = p11 / temp_bounds[1];
		proj[3][1] = -1 + proj[1][1] * (-temp_bounds[0]);
		color = config->colors.temp;
		glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, temp));
		break;
	case RH:
		proj[1][1] = p11 / rh_bounds[1];
		proj[3][1] = -1 + proj[1][1] * (-rh_bounds[0]);
		color = config->colors.rh;
		glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, rh));
		break;
	case DEWPT:
		proj[1][1] = p11 / temp_bounds[1];
		proj[3][1] = -1 + proj[1][1] * (-temp_bounds[0]);
		color = config->colors.dewpt;
		glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, dewpt));
		break;
	case ALT:
		proj[1][1] = p11 / alt_bounds[1];
		proj[3][1] = -1 + proj[1][1] * (-alt_bounds[0]);
		color = config->colors.alt;
		glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, alt));
		break;
	case PRESS:
		proj[1][1] = p11 / press_bounds[1];
		proj[3][1] = - 1 + proj[1][1] * (-press_bounds[0]);
		color = config->colors.press;
		glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, pressure));
		break;
	case HDG:
		proj[1][1] = p11 / hdg_bounds[1];
		proj[3][1] = -1 + proj[1][1] * (-hdg_bounds[0]);
		color = config->colors.hdg;
		glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, hdg));
		break;
	case SPD:
		proj[1][1] = p11 / spd_bounds[1];
		proj[3][1] = -1 + proj[1][1] * (-spd_bounds[0]);
		color = config->colors.spd;
		glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, spd));
		break;
	case CLIMB:
		proj[1][1] = p11 / climb_bounds[1];
		proj[3][1] = -1 + proj[1][1] * (-climb_bounds[0]);
		color = config->colors.climb;
		glVertexAttribPointer(ctx->attrib_pos_y, 1, GL_FLOAT, GL_FALSE, sizeof(GeoPoint), (void*)offsetof(GeoPoint, climb));
		break;
	}

	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);
	glUniform4fv(ctx->u4f_color, 1, color);
}

static void
timeseries_opengl_init(GLTimeseries *ctx)
{
	GLint status;

	const GLchar *vertex_shader = _binary_simpleproj_vert;
	const int vertex_shader_len = SYMSIZE(_binary_simpleproj_vert);
	const GLchar *fragment_shader = _binary_simplecolor_frag;
	const int fragment_shader_len = SYMSIZE(_binary_simplecolor_frag);

	/* Program + shaders */
	status = gl_compile_and_link(&ctx->program,
	                             vertex_shader, vertex_shader_len,
	                             fragment_shader, fragment_shader_len);
	if (status != GL_TRUE) log_error("Failed to compile shaders");

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
