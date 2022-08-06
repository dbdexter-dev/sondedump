#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include "decode.h"
#include "gl_ground_track.h"
#include "gui/gl_utils.h"
#include "gui/shaders/shaders.h"
#include "gui/style.h"
#include "log/log.h"
#include "utils.h"
#include "gl_map.h"

static void track_opengl_init(GLGroundTrack *map);

void
gl_ground_track_init(GLGroundTrack *ctx, float w_scale, float h_scale)
{
	track_opengl_init(ctx);

	ctx->w_scale = w_scale;
	ctx->h_scale = h_scale;
}

void
gl_ground_track_deinit(GLGroundTrack *ctx)
{
	if (ctx->program) glDeleteProgram(ctx->program);
	if (ctx->vao) glDeleteVertexArrays(1, &ctx->vao);
	if (ctx->vbo) glDeleteBuffers(1, &ctx->vbo);
}

void
gl_ground_track_set_scale(GLGroundTrack *ctx, float w_scale, float h_scale)
{
	ctx->w_scale = w_scale;
	ctx->h_scale = h_scale;
}

void
gl_ground_track_data(GLGroundTrack *ctx, const Config *conf, int width, int height, const GeoPoint *data, size_t len)
{
	float center_x = conf->map.center_x;
	float center_y = conf->map.center_y;
	float zoom = conf->map.zoom;
	const float zoom_multiplier = powf(2, zoom);
	const float track_color[] = STYLE_ACCENT_1_NORMALIZED;
	const float receiver_color[] = STYLE_ACCENT_3_NORMALIZED;

	GeoPoint receiver_loc;

	GLfloat proj[4][4] = {
		{1.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 1.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 1.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
	};

	/* Set up projection transform */
	proj[0][0] *= 2.0f * ctx->w_scale / (float)width * zoom_multiplier;
	proj[1][1] *= -2.0f * ctx->h_scale / (float)height * zoom_multiplier;
	proj[3][0] = proj[0][0] * (-center_x);
	proj[3][1] = proj[1][1] * (-center_y);

	/* Draw ground track {{{ */
	glUseProgram(ctx->program);
	glBindVertexArray(ctx->vao);
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
	glBufferData(GL_ARRAY_BUFFER, len * sizeof(GeoPoint), data, GL_STREAM_DRAW);

	/* Shader converts (lat,lon) to (x,y): translate so that the center of the
	 * viewport is (0, 0) */
	glUniformMatrix4fv(ctx->u4m_proj, 1, GL_FALSE, (GLfloat*)proj);
	glUniform4fv(ctx->u4f_color, 1, track_color);

	glDrawArrays(GL_POINTS, 0, len);
	/* }}} */
	/* Draw ground station {{{ */
	receiver_loc.lat = conf->receiver.lat;
	receiver_loc.lon = conf->receiver.lon;
	glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GeoPoint), &receiver_loc, GL_STREAM_DRAW);

	glUniform4fv(ctx->u4f_color, 1, receiver_color);
	glDrawArrays(GL_POINTS, 0, 1);
	/* }}} */

	/* Cleanup */
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

/* Static functions {{{ */
static void
track_opengl_init(GLGroundTrack *map)
{
	GLuint attrib_pos_x, attrib_pos_y;
	GLint status;

	const GLchar *vertex_shader = _binary_worldproj_vert;
	const int vertex_shader_len = SYMSIZE(_binary_worldproj_vert);
	const GLchar *fragment_shader = _binary_simplecolor_frag;
	const int fragment_shader_len = SYMSIZE(_binary_simplecolor_frag);

	/* Program + shaders */
	status = gl_compile_and_link(&map->program,
	                             vertex_shader, vertex_shader_len,
	                             fragment_shader, fragment_shader_len);
	if (status != GL_TRUE) log_error("Failed to compile shaders");

	/* Uniforms + attributes */
	map->u4m_proj = glGetUniformLocation(map->program, "u_proj_mtx");
	map->u4f_color = glGetUniformLocation(map->program, "u_color");
	attrib_pos_x = glGetAttribLocation(map->program, "in_position_x");
	attrib_pos_y = glGetAttribLocation(map->program, "in_position_y");

	log_debug("proj_mtx %d color %d",
			map->u4m_proj, map->u4f_color);
	log_debug("pos_x %d pos_y %d", attrib_pos_x, attrib_pos_y);

	glGenVertexArrays(1, &map->vao);
	glBindVertexArray(map->vao);
	glGenBuffers(1, &map->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, map->vbo);

	glEnableVertexAttribArray(attrib_pos_x);
	glEnableVertexAttribArray(attrib_pos_y);
	glVertexAttribPointer(attrib_pos_x, 1, GL_FLOAT, GL_FALSE,
	                      sizeof(GeoPoint), (void*)offsetof(GeoPoint, lat));
	glVertexAttribPointer(attrib_pos_y, 1, GL_FLOAT, GL_FALSE,
	                      sizeof(GeoPoint), (void*)offsetof(GeoPoint, lon));
}
/* }}} */
