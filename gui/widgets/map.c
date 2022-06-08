#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "map.h"
#include "gui/style.h"
#include "utils.h"

static float lat_to_y(float latitude);
static float lon_to_x(float longitude);
static void* png_load_file(const char *path);

void
widget_map_init(WidgetMap *map)
{
	glGenFramebuffers(1, &map->framebuffer);


}

void
widget_map(WidgetMap *map, struct nk_context *ctx, float lat, float lon, float zoom)
{
	char path[512];
	void *image;
	float center_x, center_y;
	struct nk_vec2 bounds;
	int i;

	bounds = nk_window_get_content_region_size(ctx);

	center_x = lon_to_x(lon);
	center_y = lat_to_y(lat);

	/* TODO remove textures that are no longer necessary (e.g. some amount of
	 * distance away from the current center) */

	/* Check whether the texture is already loaded into VRAM */
	for (i=0; i<map->count; i++) {
		if (map->coords[i].x == center_x && map->coords[i].y == center_y) {
			/* Texture loaded */
		}
	}
	if (i == map->count) {
		/* Texture not yet loaded */
		map->count++;
		map->coords = realloc(map->coords, map->count * sizeof(*map->coords));

		sprintf(path, "/home/dbdexter/Projects/magellan/out/%d/%d/%d.png", MAP_ZOOM, (int)center_y, (int)center_x);
		image = png_load_file(path);

		map->coords[i].x = center_x;
		map->coords[i].y = center_y;

		glGenTextures(1, &map->coords[i].texture_id);
		glBindTexture(GL_TEXTURE_2D, map->coords[i].texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
				(GLsizei)MAP_TILE_WIDTH, (GLsizei)MAP_TILE_HEIGHT, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, image);

	}

	/**
	 * Idea 1: compute the location of each image in software, pass those
	 * onto the renderer
	 */

	/**
	 * Idea 2: merge images into one texture, compute transformation matrix
	 * and somehow pass that onto the renderer
	 */

}


/* Static functions {{{ */
static float
lat_to_y(float lat)
{
	return (1 << MAP_ZOOM) * (90 - lat) / 180.0;
}

static float
lon_to_x(float lon)
{
	const float lon_rad = lon * M_PI/180.0;

	return (1 << MAP_ZOOM)
	       * (1.0 - logf(tanf(M_PI/4 + lon_rad/2)) / M_PI)
	       * 0.5;
}

static void*
png_load_file(const char *path)
{
}
/* }}} */
