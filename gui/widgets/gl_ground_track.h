#ifndef gl_ground_track_h
#define gl_ground_track_h

#include "libs/glad/glad.h"
#include "config.h"
#include "decode.h"

typedef struct {
	float w_scale, h_scale;
	GLuint vao, vbo;
	GLuint program;

	GLuint u4m_proj, u4f_color;
} GLGroundTrack;

void gl_ground_track_init(GLGroundTrack *ctx, float w_scale, float h_scale);
void gl_ground_track_deinit(GLGroundTrack *ctx);
void gl_ground_track_set_scale(GLGroundTrack *ctx, float w_scale, float h_scale);

/**
 * Render a ground track
 *
 * @param ctx       map context
 * @param config    global config
 * @param width     viewport width, in pixels
 * @param height    viewport height, in pixels
 * @param data      data to display
 * @param len       number of elements to display in the data array
 */
void gl_ground_track_data(GLGroundTrack *ctx, const Config *conf, int width, int height, const GeoPoint *data, size_t len);


#endif
