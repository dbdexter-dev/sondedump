#ifndef gui_h
#define gui_h

#include "utils.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define WINDOW_TITLE "SondeDump v" VERSION

enum graph {
	GUI_TIMESERIES=0,
	GUI_SKEW_T,
	GUI_OPENSTREETMAP,
};

void gui_init(void);
void gui_deinit(void);

enum graph gui_get_graph(void);
void       gui_set_graph(enum graph visual);

#endif

