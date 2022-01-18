#include <curses.h>
#include <locale.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "gps/ecef.h"
#include "physics.h"
#include "tui.h"
#include "utils.h"

#define DEFAULT_UPD_INTERVAL 100

#define PTU_INFO_COUNT 5    /* Calib percent, Temp, RH, pressure, dewpt */
#define GPS_INFO_COUNT 6    /* lat, lot, alt, speed, heading, climb */
#define SONDE_INFO_COUNT 4  /* Serial, burstkill, frame seq, date/time */
#define XDATA_INFO_COUNT 1  /* xdata */
#define INFO_COUNT (PTU_INFO_COUNT + GPS_INFO_COUNT + SONDE_INFO_COUNT + XDATA_INFO_COUNT)

static void init_windows();
static void redraw();
static void draw_tabs(WINDOW *win, int selected);
static void handle_resize();
static void *main_loop(void* args);

static char *_decoder_names[] = {"RS41", "DFM", "M10", "iMS100"};
static int _update_interval;
static int _running;
static pthread_t _tid;
static struct {
	WINDOW *win;
	WINDOW *tabs;
	PrintableData data;
	int data_changed;
	int receiver_location_set;
	float lat, lon, alt;
	int (*get_active_decoder)(void);
	enum { ABSOLUTE=0, RELATIVE, POS_TYPE_COUNT } pos_type;
} tui;


void
tui_init(int update_interval, void (*decoder_changer)(int index), int (*get_active_decoder)(void))
{
	setlocale(LC_ALL, "");

	_update_interval = (update_interval > 0 ? update_interval : DEFAULT_UPD_INTERVAL);

	initscr();
	noecho();
	cbreak();
	curs_set(0);

	use_default_colors();
	start_color();

	tui.lat = tui.lon = tui.alt = 0;
	tui.pos_type = ABSOLUTE;
	tui.receiver_location_set = 0;
	tui.data_changed = 1;
	tui.get_active_decoder = get_active_decoder;

	init_windows();
	keypad(tui.win, 1);

	memset(&tui.data, 0, sizeof(tui.data));

	_running = 1;
	pthread_create(&_tid, NULL, main_loop, decoder_changer);
}

void
tui_deinit()
{
	_running = 0;
	pthread_join(_tid, NULL);
	delwin(tui.win);
	endwin();
}

void
tui_set_ground_location(float lat, float lon, float alt)
{
	tui.lat = lat;
	tui.lon = lon;
	tui.alt = alt;
	tui.receiver_location_set = 1;
}


int
tui_update(PrintableData *data)
{
	tui.data = *data;
	tui.data_changed = 1;

	return 0;
}

/* Static functions {{{ */
static void*
main_loop(void *args)
{
	int ch;
	void (*decoder_changer)(int index) = args;
	int new_decoder;

	while (_running) {
		switch (ch = wgetch(tui.win)) {
			case KEY_RESIZE:
				handle_resize();
				break;
			case KEY_LEFT:
				new_decoder = (tui.get_active_decoder() - 1) % LEN(_decoder_names);
				memset(&tui.data, 0, sizeof(tui.data));
				decoder_changer(new_decoder);
				tui.data_changed = 1;
				break;
			case KEY_RIGHT:
				new_decoder = (tui.get_active_decoder() + 1) % LEN(_decoder_names);
				memset(&tui.data, 0, sizeof(tui.data));
				decoder_changer(new_decoder);
				tui.data_changed = 1;
				break;
			case '\t':
				if (tui.receiver_location_set) {
					tui.pos_type = (tui.pos_type + 1) % POS_TYPE_COUNT;
					tui.data_changed = 1;
				}
				break;
			case KEY_BTAB:
				if (tui.receiver_location_set) {
					tui.pos_type = (tui.pos_type - 1 + POS_TYPE_COUNT) % POS_TYPE_COUNT;
					tui.data_changed = 1;
				}
				break;
			default:
				break;
		}

		if (tui.data_changed) {
			redraw();
			mvwprintw(tui.win, 1, 1, "*");
			tui.data_changed = 0;
		} else {
			mvwprintw(tui.win, 1, 1, " ");
		}
	}
	return NULL;
}

static void
handle_resize()
{
	const int width = 50;
	const int height = INFO_COUNT + 3 + 3;
	int rows, cols;
	werase(stdscr);
	endwin();

	refresh();
	getmaxyx(stdscr, rows, cols);

	if (!tui.win) {
		tui.win = newwin(height, width, (rows - height) / 2 + 1, (cols - width) / 2);
	} else {
		wresize(tui.win, height, width);
		mvwin(tui.win, (rows - height) / 2 + 1, (cols - width) / 2);
	}
	if (!tui.tabs) {
		tui.tabs = newwin(2, width, (rows - height) / 2 - 1, (cols - width) / 2);
	} else {
		wresize(tui.tabs, 2, width);
		mvwin(tui.tabs, (rows - height) / 2 - 1, (cols - width) / 2);
	}
	redraw();
}

static void
redraw()
{
	int rows, cols;
	int start_row, start_col;
	char time[64], shutdown_timer[32];
	float az, el, slant;

	/* Draw tabs at top of the TUI */
	draw_tabs(tui.tabs, tui.get_active_decoder());

	/* Format data to be printable */
	strftime(time, LEN(time), "%a %b %d %Y %H:%M:%S", gmtime(&tui.data.utc_time));
	if (tui.data.shutdown_timer > 0) {
		sprintf(shutdown_timer, "%d:%02d:%02d",
				tui.data.shutdown_timer/3600,
				tui.data.shutdown_timer/60%60,
				tui.data.shutdown_timer%60
				);
	} else {
		shutdown_timer[0] = 0;
	}

	getmaxyx(tui.win, rows, cols);
	start_row = (rows - INFO_COUNT - 4) / 2;
	start_col = cols / 2 - 1;

	werase(tui.win);
	wborder(tui.win,
			0, 0, 0, 0,
			0, 0, 0, 0);

	mvwprintw(tui.win, start_row++, start_col - sizeof("Serial no.:"),
			"Serial no.: %s", tui.data.serial);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Frame no.:"),
			"Frame no.: %d", tui.data.seq);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Onboard time:"),
			"Onboard time: %s", time);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Shutdown in:"),
			"Shutdown in: %s", shutdown_timer);
	start_row++;

	switch (tui.pos_type) {
		case ABSOLUTE:
			mvwprintw(tui.win, start_row++, start_col - sizeof("Latitude:"),
					"Latitude: %.5f%c", fabs(tui.data.lat), tui.data.lat >= 0 ? 'N' : 'S');
			mvwprintw(tui.win, start_row++, start_col - sizeof("Longitude:"),
					"Longitude: %.5f%c", fabs(tui.data.lon), tui.data.lon >= 0 ? 'E' : 'W');
			mvwprintw(tui.win, start_row++, start_col - sizeof("Altitude:"),
					"Altitude: %.0fm", tui.data.alt);
			break;
		case RELATIVE:
			lla_to_aes(&az, &el, &slant,
			           tui.data.lat, tui.data.lon, tui.data.alt,
			           tui.lat, tui.lon, tui.alt);

			mvwprintw(tui.win, start_row++, start_col - sizeof("Azimuth:"),
					"Azimuth: %.1f'", az);
			mvwprintw(tui.win, start_row++, start_col - sizeof("Elevation:"),
					"Elevation: %.2f'", el);
			mvwprintw(tui.win, start_row++, start_col - sizeof("Slant range:"),
					"Slant range: %.2fkm", slant / 1000.0);
			break;
		default:
			tui.pos_type = ABSOLUTE;
			break;
	}

	mvwprintw(tui.win, start_row++, start_col - sizeof("Speed:"),
			"Speed: %.1fm/s", tui.data.speed);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Heading:"),
			"Heading: %.0f'", tui.data.heading);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Climb:"),
			"Climb: %+.1fm/s", tui.data.climb);
	start_row++;

	mvwprintw(tui.win, start_row++, start_col - sizeof("Calibration:"),
			"Calibration: %.0f%%", floorf(tui.data.calib_percent));
	mvwprintw(tui.win, start_row++, start_col - sizeof("Temperature:"),
			"Temperature: %.1f'C", tui.data.temp);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Rel. humidity:"),
			"Rel. humidity: %.0f%%", tui.data.rh);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Dew point:"),
			"Dew point: %.1f'C", dewpt(tui.data.temp, tui.data.rh));
	mvwprintw(tui.win, start_row++, start_col - sizeof("Pressure:"),
			"Pressure: %.1fhPa", tui.data.pressure);

	start_row++;
	mvwprintw(tui.win, start_row++, start_col - sizeof("Aux. data:"),
			"Aux. data: %s", tui.data.xdata);
	start_row++;

	wrefresh(tui.win);
}

static void
draw_tabs(WINDOW *win, int selected)
{
	float elemWidth;
	int i;
	int cols;

	werase(win);
	cols = getmaxx(win);
	elemWidth = (float)cols / LEN(_decoder_names);
	wborder(tui.tabs,
			0, 0, 0, ' ',
			0, 0, ACS_VLINE, ACS_VLINE);

	for (i=0; i<(int)LEN(_decoder_names); i++) {
		if (i != 0) mvwprintw(win, 1, i * elemWidth, "|");

		if (i == selected) wattron(win, A_STANDOUT);
		mvwprintw(win, 1, i * elemWidth + roundf((elemWidth - strlen(_decoder_names[i])) / 2), "%s",_decoder_names[i]);
		if (i == selected) wattroff(win, A_STANDOUT);
	}

	wrefresh(win);
}

static void
init_windows()
{
	handle_resize();
	wborder(tui.win,
			0, 0, 0, 0,
			0, 0, 0, 0);
	wborder(tui.tabs,
			0, 0, 0, ' ',
			0, 0, ACS_VLINE, ACS_VLINE);
	wtimeout(tui.win, _update_interval);
	wrefresh(tui.win);
	wrefresh(tui.tabs);
}

/* }}} */
