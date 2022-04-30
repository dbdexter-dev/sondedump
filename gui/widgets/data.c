#include <math.h>
#include <stdio.h>
#include "data.h"
#include "decode.h"
#include "include/data.h"

void
widget_data(struct nk_context *ctx, int width, int height)
{
	char tmp[32];
	(void) width;
	(void) height;

	PrintableData *printable = get_data();

	nk_layout_row_dynamic(ctx, DATA_ITEM_HEIGHT, 9);

	sprintf(tmp, "%.5f%c", fabs(printable->lat), printable->lat >= 0 ? 'N' : 'S');
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	sprintf(tmp, "%.5f%c", fabs(printable->lon), printable->lon >= 0 ? 'E' : 'W');
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	sprintf(tmp, "%.0f m", printable->alt);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	sprintf(tmp, "%.1f m/s", printable->speed);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	sprintf(tmp, "%.0f'", printable->heading);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	sprintf(tmp, "%+.1f m/s", printable->climb);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	sprintf(tmp, "%.1f'C", printable->temp);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	sprintf(tmp, "%.0f%%", printable->rh);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	sprintf(tmp, "%.1f hPa", printable->pressure);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

}
