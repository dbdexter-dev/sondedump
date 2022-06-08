#ifndef gui_map_h
#define gui_map_h

#include <GL/gl.h>
#include "nuklear/nuklear.h"

#define MAP_ZOOM 8
#define MAP_TILE_WIDTH 256
#define MAP_TILE_HEIGHT 256

typedef struct {
	GLuint framebuffer;

	struct {
		int x, y;
		unsigned int texture_id;
	} *coords;

	int count;
} WidgetMap;

void widget_map(WidgetMap *map, struct nk_context *ctx, float lat, float lon, float zoom);

#endif
