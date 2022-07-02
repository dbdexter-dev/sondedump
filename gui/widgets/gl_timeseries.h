#ifndef gl_timeseries_h
#define gl_timeseries_h

#include <GLES3/gl3.h>

typedef struct {
	GLuint vao, vbo;

	GLuint program;
	GLuint vert_shader, frag_shader;

	GLuint u4m_proj;
	GLuint u4f_color;
	GLuint attrib_pos_x, attrib_pos_y;
} GLTimeseries;

void gl_timeseries_init(GLTimeseries *ctx);
void gl_timeseries_deinit(GLTimeseries *ctx);
void gl_timeseries(GLTimeseries *ctx);

#endif
