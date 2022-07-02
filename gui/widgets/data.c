#include <math.h>
#include <stdio.h>
#include <time.h>
#include "data.h"
#include "decode.h"
#include "include/data.h"
#include "physics.h"
#include "style.h"


extern const char *_decoder_names[];
extern const int _decoder_count;

int
widget_data(struct nk_context *ctx, float scale)
{
	char tmp[128];

	PrintableData *printable = get_data();

	nk_layout_row_dynamic(ctx, STYLE_DEFAULT_ROW_HEIGHT * scale, 2);

	nk_label(ctx, "Serial no.:", NK_TEXT_RIGHT);
	nk_label(ctx, printable->serial, NK_TEXT_LEFT);

	nk_label(ctx, "Frame no.:", NK_TEXT_RIGHT);
	sprintf(tmp, "%d", printable->seq);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Onboard time:", NK_TEXT_RIGHT);
	strftime(tmp, sizeof(tmp), "%a %b %d %Y %H:%M:%S", gmtime(&printable->utc_time));
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "", NK_TEXT_LEFT);
	nk_label(ctx, "", NK_TEXT_LEFT);

	nk_label(ctx, "Latitude: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%.5f%c", fabs(printable->lat), printable->lat >= 0 ? 'N' : 'S');
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Longitude: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%.5f%c", fabs(printable->lon), printable->lon >= 0 ? 'E' : 'W');
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Altitude: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%.0f m", printable->alt);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Speed: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%.1f m/s", printable->speed);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Heading: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%.0f'", printable->heading);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Climb: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%+.1f m/s", printable->climb);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "", NK_TEXT_LEFT);
	nk_label(ctx, "", NK_TEXT_LEFT);

	nk_label(ctx, "Temperature: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%.1f'C", printable->temp);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Humidity: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%.0f%%", printable->rh);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Dew point: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%.1f'C", dewpt(printable->temp, printable->rh));
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Pressure: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%.1f hPa", printable->pressure);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Aux. data: ", NK_TEXT_RIGHT);
	sprintf(tmp, "%s", printable->xdata);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	nk_label(ctx, "Calibration:", NK_TEXT_RIGHT);
	sprintf(tmp, "%.0f%%", printable->calib_percent);
	nk_label(ctx, tmp, NK_TEXT_LEFT);

	return 0;
}
