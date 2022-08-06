#ifndef gl_timeseries_h
#define gl_timeseries_h

#include <stdlib.h>
#include "config.h"
#include "decode.h"
#include "libs/glad/glad.h"

typedef struct {
	GLuint vao, vbo;

	GLuint program;

	GLuint u4m_proj;
	GLuint u4f_color;
	GLuint attrib_pos_x, attrib_pos_y;
} GLTimeseries;

void gl_timeseries_init(GLTimeseries *ctx);
void gl_timeseries_deinit(GLTimeseries *ctx);

void gl_timeseries_ptu(GLTimeseries *ctx, const Config *config, const GeoPoint *data, size_t len, const GeoPoint *maxima, const GeoPoint *minima);
void gl_timeseries_gps(GLTimeseries *ctx, const Config *config, const GeoPoint *data, size_t len, const GeoPoint *maxima, const GeoPoint *minima);

#endif
