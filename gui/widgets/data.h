#ifndef widget_data_h
#define widget_data_h

#include "nuklear/nuklear.h"

float widget_data_base_size(struct nk_context *ctx, float lat, float lon, float alt);
int widget_data(struct nk_context *ctx, float lat, float lon, float alt, float scale);

#endif
