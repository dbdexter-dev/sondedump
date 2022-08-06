#ifndef gui_state_h
#define gui_state_h

#include "config.h"
#include "gui.h"
#include "widgets/gl_ground_track.h"
//#include "widgets/gl_map.h"
#include "widgets/gl_map_raster.h"
#include "widgets/gl_skewt.h"
#include "widgets/gl_timeseries.h"

typedef struct {
	struct {
		float x, y;
	} mouse;
	float old_scale;
	struct nk_colorf *picked_color;

	int config_open, export_open;
	int over_window;
	int dragging;
	int force_refresh;

	char lat[32], lon[32], alt[32], tile_endpoint[URL_MAXLEN];

	enum graph active_widget;

	GLRasterMap raster_map;
	//GLMap map;
	GLGroundTrack ground_track;
	GLSkewT skewt;
	GLTimeseries timeseries;
} UIState;

#endif
