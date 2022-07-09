#ifndef gui_h
#define gui_h

#include "utils.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define PANEL_WIDTH 400
#define WINDOW_TITLE "SondeDump v" VERSION

enum graph {
	GUI_TIMESERIES_PTU, /* Received PTU data as a graph over time */
	GUI_TIMESERIES_GPS, /* Received GPS data as a graph over time */
	GUI_SKEW_T,         /* Received temp + dewpt on a skew-t background */
	GUI_MAP,            /* Word map showing the ground track */
};

/**
 * Initialize SDL2 GUI. Will launch a new thread to handle GUI inputs
 */
void gui_init(void);

/**
 * Halt GUI execution and exit gracefully
 */
void gui_deinit(void);

/**
 * Force the GUI to update even though no input was given by the user
 */
void gui_force_update(void);

#endif

