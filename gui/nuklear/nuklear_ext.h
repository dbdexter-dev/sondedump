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


#endif
