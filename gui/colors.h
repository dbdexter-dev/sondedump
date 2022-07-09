#ifndef gui_colors_h
#define gui_colors_h

enum gui_color {
	COLOR_TEMP=0,
	COLOR_RH=1,
	COLOR_DEWPT=2,
	COLOR_ALT=3,
	COLOR_SPD=4,
	COLOR_HDG=5,
	COLOR_CLIMB=6,
	COLOR_PRESS=7,

	COLOR_COUNT
};


const float *get_gui_color(enum gui_color id);
void set_gui_color(enum gui_color id, const float color[4]);
void default_gui_colors(void);

#endif
