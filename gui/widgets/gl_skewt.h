#ifndef gl_skewt_h
#define gl_skewt_h

#include <GLES2/gl2.h>

typedef struct {
	float center_x, center_y, zoom;

	GLuint vao, vbo, ibo;
	GLuint cp_vbo;          /* Bezier control points */
	GLuint data_vao, data_vbo;

	GLuint chart_program;
	GLuint chart_vert_shader, chart_frag_shader;
	GLuint data_program;
	GLuint data_vert_shader, data_frag_shader;

	GLuint u4m_proj, u4f_color, u1f_thickness, u1f_frag_thickness;
	GLuint u4f_data_color, u4m_data_proj;

} GLSkewT;

void gl_skewt_init(GLSkewT *ctx);
void gl_skewt_deinit(GLSkewT *ctx);
void gl_skewt_vector(GLSkewT *ctx, float width, float height);


#endif
