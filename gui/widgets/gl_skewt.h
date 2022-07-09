#ifndef gl_skewt_h
#define gl_skewt_h

#include <stdlib.h>
#include "config.h"
#include "decode.h"
#include "libs/glad/glad.h"

typedef struct {
	float center_x, center_y, zoom;

	GLuint vao, vbo, ibo[16];           /* Bezier background */
	GLuint data_vao, data_vbo;          /* Data points */

	GLuint chart_program;
	GLuint chart_vert_shader, chart_frag_shader;
	GLuint data_program;
	GLuint data_vert_shader, data_frag_shader;

	GLuint u4m_proj, u4f_color, u1f_thickness, u1f_frag_thickness;
	GLuint u4f_data_color, u1f_data_zoom, u4m_data_proj;
	GLuint attrib_data_temp;

} GLSkewT;

/**
 * Initialize a OpenGL-based skew-t plot
 *
 * @param ctx skew-t context, will be configured by the init function
 */
void gl_skewt_init(GLSkewT *ctx);

/**
 * Deinitialize a skew-t plot
 *
 * @param ctx context to deinitialize
 */
void gl_skewt_deinit(GLSkewT *ctx);

/**
 * Draw a skew-t plot, together with recorded data
 *
 * @param ctx       skew-t context
 * @param width     viewport width, in pixels
 * @param height    viewport height, in pixels
 * @param data      data to display
 * @param len       number of elements to display in the data array
 */
void gl_skewt_vector(GLSkewT *ctx, const Config *config, int width, int height, const GeoPoint *data, size_t len);


#endif
