#include <string.h>
#include "colors.h"
#include "style.h"

static float colors[COLOR_COUNT][4];

const float*
get_gui_color(enum gui_color id)
{
	return colors[id];
}

void
set_gui_color(enum gui_color id, const float color[4])
{
	memcpy(colors[id], color, sizeof(colors[id]));
}

void
default_gui_colors(void)
{
	const float temperature_color[] = STYLE_ACCENT_1_NORMALIZED;
	const float dewpt_color[] = STYLE_ACCENT_0_NORMALIZED;
	const float alt_color[] = STYLE_ACCENT_3_NORMALIZED;
	const float rh_color[] = STYLE_ACCENT_2_NORMALIZED;
	const float press_color[] = {133/255.0, 44/255.0, 35/255.0, 1};
	const float climb_color[] = {64/255.0, 108/255.0, 30/255.0, 1.0};
	const float hdg_color[] = {220/255.0, 102/255.0, 217/255.0, 1.0};
	const float spd_color[] = {161/255.0, 101/255.0, 259/255.0, 1.0};

	memcpy(colors[COLOR_TEMP], temperature_color, sizeof(temperature_color));
	memcpy(colors[COLOR_RH], rh_color, sizeof(rh_color));
	memcpy(colors[COLOR_DEWPT], dewpt_color, sizeof(dewpt_color));
	memcpy(colors[COLOR_ALT], alt_color, sizeof(alt_color));
	memcpy(colors[COLOR_PRESS], press_color, sizeof(press_color));
	memcpy(colors[COLOR_SPD], spd_color, sizeof(spd_color));
	memcpy(colors[COLOR_HDG], hdg_color, sizeof(hdg_color));
	memcpy(colors[COLOR_CLIMB], climb_color, sizeof(climb_color));
}
