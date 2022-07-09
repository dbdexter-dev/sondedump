#ifndef nuklear_ext_h
#define nuklear_ext_h

#include "nuklear.h"

/**
 * Adapt window height to fit all of its contents
 * @param ctx nuklear context
 * @note call this function at the very end of the layout of a window, before
 *       closing the if statement guarding the window being displayed
 */
void nk_window_fit_to_content(struct nk_context *ctx);

/**
 * Just like nk_button_color but accepting 0..1 float values instead of 0..255
 * uint8_ts
 *
 * @param   ctx nuklear context
 * @param   color color quadruplet (r,g,b,a)
 * @return  true if clicked, false otherwise
 */
nk_bool nk_button_colorf(struct nk_context *ctx, struct nk_colorf color);


#endif
