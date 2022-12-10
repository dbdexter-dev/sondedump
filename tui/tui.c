#include <curses.h>
#include <locale.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "gps/ecef.h"
#include "decode.h"
#include "physics.h"
#include "tui.h"
#include "utils.h"

#define DEFAULT_UPD_INTERVAL 100

#define PTU_INFO_COUNT 5    /* Calib percent, Temp, RH, pressure, dewpt */
#define GPS_INFO_COUNT 6    /* lat, lot, alt, speed, heading, climb */
#define SONDE_INFO_COUNT 4  /* Serial, burstkill, frame seq, date/time */
#define XDATA_INFO_COUNT 1  /* xdata */
#define INFO_COUNT (PTU_INFO_COUNT + GPS_INFO_COUNT + SONDE_INFO_COUNT + XDATA_INFO_COUNT)

extern char *_decoder_names[];
extern int _decoder_count;

static void init_windows(int update_interval);
static void redraw(void);
static void draw_tabs(WINDOW *win, int selected);
static void handle_resize(void);
static void *main_loop(void *args);

static int _running;
static pthread_t _tid;
static struct {
	WINDOW *win;
	WINDOW *tabs;
	int last_slot;
	int receiver_location_set;
	float lat, lon, alt;
	enum { ABSOLUTE=0, RELATIVE, POS_TYPE_COUNT } pos_type;
} tui;


void
tui_init(int update_interval)
{
	update_interval = (update_interval > 0 ? update_interval : DEFAULT_UPD_INTERVAL);

	setlocale(LC_ALL, "");

	initscr();
	noecho();
	cbreak();
	curs_set(0);

	use_default_colors();
	start_color();

	tui.pos_type = ABSOLUTE;
	tui.receiver_location_set = 0;
	tui.last_slot = -1;

	init_windows(update_interval);
	keypad(tui.win, 1);

	_running = 1;
	pthread_create(&_tid, NULL, main_loop, NULL);
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


/* Static functions {{{ */
static void*
main_loop(void *args)
{
	int ch;

	(void)args;

	while (_running) {
		switch (ch = wgetch(tui.win)) {
		case KEY_RESIZE:
			handle_resize();
			break;
		case KEY_LEFT:
		case '<':
			set_active_decoder(get_active_decoder() - 1);
			tui.last_slot = -1;
			break;
		case KEY_RIGHT:
		case '>':
			set_active_decoder(get_active_decoder() + 1);
			tui.last_slot = -1;
			break;
		case '\t':
			if (tui.receiver_location_set) {
				tui.pos_type = (tui.pos_type + 1) % POS_TYPE_COUNT;
				tui.last_slot = -1;
			}
			break;
		case KEY_BTAB:
			if (tui.receiver_location_set) {
				tui.pos_type = (tui.pos_type - 1 + POS_TYPE_COUNT) % POS_TYPE_COUNT;
				tui.last_slot = -1;
			}
			break;
		default:
			break;
		}

		if (tui.last_slot != get_slot()) {
			tui.last_slot = get_slot();
			redraw();
			mvwprintw(tui.win, 1, 1, "*");
		} else {
			mvwprintw(tui.win, 1, 1, " ");
		}
	}

	return NULL;
}

static void
handle_resize(void)
{
	const int width = 55;
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
redraw(void)
{
	int rows, cols;
	int start_row, start_col;
	char time[64], shutdown_timer[32];
	float az, el, slant;
<<<<<<< HEAD
	PrintableData *data = get_data();
=======
	const SondeData *data = get_data();
>>>>>>> 0998189 (Overhauled data exchange format)

	/* Draw tabs at top of the TUI */
	draw_tabs(tui.tabs, get_active_decoder());

	/* Format data to be printable */
	if (data->fields & DATA_TIME)
		strftime(time, LEN(time), "%a %b %d %Y %H:%M:%S", gmtime(&data->time));
	if (data->fields & DATA_SHUTDOWN) {
		sprintf(shutdown_timer, "%d:%02d:%02d",
				data->shutdown/3600,
				data->shutdown/60%60,
				data->shutdown%60
				);
	} else {
		shutdown_timer[0] = 0;
	}

	getmaxyx(tui.win, rows, cols);
	start_row = (rows - INFO_COUNT - 4) / 2;
	start_col = cols / 2 + 1;

	werase(tui.win);
	wborder(tui.win,
			0, 0, 0, 0,
			0, 0, 0, 0);

	mvwprintw(tui.win, start_row++, start_col - sizeof("Serial no.:"),
			"Serial no.: %s", data->serial);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Frame no.:"),
			"Frame no.: %d", data->seq);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Onboard time:"),
			"Onboard time: %s", time);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Shutdown in:"),
			"Shutdown in: %s", shutdown_timer);
	start_row++;

	switch (tui.pos_type) {
	case ABSOLUTE:
		mvwprintw(tui.win, start_row++, start_col - sizeof("Latitude:"),
				"Latitude: %.5f%c", fabs(data->lat), data->lat >= 0 ? 'N' : 'S');
		mvwprintw(tui.win, start_row++, start_col - sizeof("Longitude:"),
				"Longitude: %.5f%c", fabs(data->lon), data->lon >= 0 ? 'E' : 'W');
		mvwprintw(tui.win, start_row++, start_col - sizeof("Altitude:"),
				"Altitude: %.0fm", data->alt);
		break;
	case RELATIVE:
		lla_to_aes(&az, &el, &slant,
				   data->lat, data->lon, data->alt,
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
			"Speed: %.1fm/s", data->speed);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Heading:"),
			"Heading: %.0f'", data->heading);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Climb:"),
			"Climb: %+.1fm/s", data->climb);
	start_row++;

	mvwprintw(tui.win, start_row++, start_col - sizeof("Calibration:"),
			"Calibration: %.0f%%", floorf(data->calib_percent));
	mvwprintw(tui.win, start_row++, start_col - sizeof("Temperature:"),
			"Temperature: %.1f'C", data->temp);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Rel. humidity:"),
			"Rel. humidity: %.0f%%", data->rh);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Dew point:"),
			"Dew point: %.1f'C", dewpt(data->temp, data->rh));
	mvwprintw(tui.win, start_row++, start_col - sizeof("Pressure:"),
			"Pressure: %.1fhPa", data->pressure);

	start_row++;
	mvwprintw(tui.win, start_row++, start_col - sizeof("Aux. data:"),
			"Aux. data: %s", data->xdata);
	start_row++;

	wrefresh(tui.win);
}

static void
draw_tabs(WINDOW *win, int selected)
{
	int i, elem_idx;
	int cols;

	werase(win);
	cols = getmaxx(win);
	wborder(tui.tabs,
			0, 0, 0, ' ',
			0, 0, ACS_VLINE, ACS_VLINE);

	mvwprintw(win, 1, 2, "<");
	mvwprintw(win, 1, cols - 3, ">");

	for (i=-1; i<2; i++) {
		elem_idx = (selected + i + _decoder_count) % _decoder_count;
		if (i == 0) wattron(win, A_STANDOUT);
		mvwprintw(win, 1, cols/2.0 + i * cols/4.0 - roundf(strlen(_decoder_names[elem_idx]) / 2.0), "%s", _decoder_names[elem_idx]);
		if (i == 0) wattroff(win, A_STANDOUT);
	}

	wrefresh(win);
}

static void
init_windows(int update_interval)
{
	handle_resize();
	wborder(tui.win,
			0, 0, 0, 0,
			0, 0, 0, 0);
	wborder(tui.tabs,
			0, 0, 0, ' ',
			0, 0, ACS_VLINE, ACS_VLINE);
	wtimeout(tui.win, update_interval);
	wrefresh(tui.win);
	wrefresh(tui.tabs);
}

/* }}} */
