#ifndef stuve_h
#define stuve_h

#include <cairo/cairo.h>
#define STUVE_ISOTHERM_TICKSTEP 10
#define STUVE_ISOBAR_TICKSTEP 50

/**
 * Draw the backdrop of a Stuve diagram
 *
 * @param surface the target surface to draw to
 * @param min_temp minimum temperature to display (x-axis lower bound)
 * @param max_temp maximum temperature (x-axis upper bound)
 * @param min_press minimum pressure to display (y-axis upper bound)
 * @param max_press maximum pressure to display (y-axis lower bound)
 */
int stuve_draw_backdrop(cairo_surface_t *surface, float t_min, float t_max, float p_min, float p_max);

/**
 * Add a datapoint to the Stuve diagram
 *
 * @param surface the surface to draw the points to
 * @param temp temperature
 * @param press pressure
 * @param dewpt dew point
 */
int stuve_draw_point(cairo_surface_t *surface, float t_min, float t_max, float p_min, float p_max, float temp, float press, float dewpt);


#endif /* stuve_h */
