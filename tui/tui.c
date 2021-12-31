#include <locale.h>
#include <math.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "tui.h"
#include "gps/ecef.h"
#include "utils.h"

#define DEFAULT_UPD_INTERVAL 100

#define PTU_INFO_COUNT 4    /* Temp, RH, pressure, dewpt */
#define GPS_INFO_COUNT 6    /* lat, lot, alt, speed, heading, climb */
#define SONDE_INFO_COUNT 4  /* Serial, burstkill, frame seq, date/time */
#define XDATA_INFO_COUNT 1  /* xdata */
#define INFO_COUNT (PTU_INFO_COUNT + GPS_INFO_COUNT + SONDE_INFO_COUNT + XDATA_INFO_COUNT)

static void init_windows(int rows, int cols);
static void redraw();
static void draw_tabs(WINDOW *win, int selected);
static void handle_resize();
static void *main_loop(void* args);

static char *_decoder_names[] = { "RS41", "DFM", "M10" };
static int _update_interval;
static int _running;
static pthread_t _tid;
static struct {
	WINDOW *win;
	WINDOW *tabs;
	struct {
		int seq;
		float temp, rh, pressure;
		float lat, lon, alt, spd, hdg, climb;
		char serial[8+1];
		char time[32];
		char shutdown_timer[16];
		char xdata[128];
		int changed;
	} data;
	int active_decoder;
} tui;


void
tui_init(int update_interval, void (*decoder_changer)(int delta))
{
	int rows, cols;
	setlocale(LC_ALL, "");

	_update_interval = (update_interval > 0 ? update_interval : DEFAULT_UPD_INTERVAL);

	initscr();
	noecho();
	cbreak();
	curs_set(0);

	use_default_colors();
	start_color();

	getmaxyx(stdscr, rows, cols);
	init_windows(rows, cols);
	keypad(tui.win, 1);

	tui.data.serial[0] = 0;
	tui.data.time[0] = 0;
	tui.data.shutdown_timer[0] = 0;

	_running = 1;
	tui.data.changed = 1;
	tui.active_decoder = 0;
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


int
tui_update(SondeData *data)
{
	switch (data->type) {
		case EMPTY:
		case FRAME_END:
		case UNKNOWN:
		case SOURCE_END:
			return 0;
		case DATETIME:
			strftime(tui.data.time, LEN(tui.data.time), "%a %b %d %Y %H:%M:%S", gmtime(&data->data.datetime.datetime));
			break;
		case INFO:
			strncpy(tui.data.serial, data->data.info.sonde_serial, LEN(tui.data.serial)-1);
			if (data->data.info.burstkill_status > 0) {
				sprintf(tui.data.shutdown_timer, "%d:%02d:%02d",
						data->data.info.burstkill_status/3600,
						data->data.info.burstkill_status/60%60,
						data->data.info.burstkill_status%60
						);
			}
			tui.data.seq = data->data.info.seq;
			break;
		case PTU:
			tui.data.temp = data->data.ptu.temp;
			tui.data.rh = data->data.ptu.rh;
			tui.data.pressure = data->data.ptu.pressure;
			break;
		case POSITION:
			tui.data.lat = data->data.pos.lat;
			tui.data.lon = data->data.pos.lon;
			tui.data.alt = data->data.pos.alt;
			tui.data.spd = data->data.pos.speed;
			tui.data.hdg = data->data.pos.heading;
			tui.data.climb = data->data.pos.climb;
			break;
		case XDATA:
			strcpy(tui.data.xdata, data->data.xdata.data);
			break;
		default:
			break;

	}
	tui.data.changed = 1;
	return 0;
}

int
tui_set_active_decoder(int active_decoder)
{
	tui.active_decoder = active_decoder;
	draw_tabs(tui.tabs, active_decoder);
	return 0;
}

/* Static functions {{{ */
static void*
main_loop(void *args)
{
	void (*decoder_changer)(int delta) = args;
	while (_running) {
		switch (wgetch(tui.win)) {
			case KEY_RESIZE:
				handle_resize();
				break;
			case KEY_LEFT:
				decoder_changer(-1);
				break;
			case KEY_RIGHT:
			case '\t':
				decoder_changer(+1);
				break;
			default:
				break;
		}

		if (tui.data.changed) {
			redraw();
			mvwprintw(tui.win, 1, 1, "*");
			tui.data.changed = 0;
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
	wresize(tui.win, height, width);
	wresize(tui.tabs, 2, width);
	mvwin(tui.win, (rows - height) / 2, (cols - width) / 2);
	mvwin(tui.tabs, (rows - height) / 2 - 2, (cols - width) / 2);
	redraw();
}
static void
redraw()
{
	int rows, cols;
	int start_row, start_col;
	float synthetic_pressure;

	draw_tabs(tui.tabs, tui.active_decoder);

	getmaxyx(tui.win, rows, cols);
	start_row = (rows - INFO_COUNT - 4) / 2;
	start_col = cols / 2 - 1;

	synthetic_pressure = (isnormal(tui.data.pressure) ?
			tui.data.pressure : altitude_to_pressure(tui.data.alt));

	werase(tui.win);
	wborder(tui.win,
			0, 0, 0, 0,
			0, 0, 0, 0);

	mvwprintw(tui.win, start_row++, start_col - sizeof("Serial no.:"),
			"Serial no.: %s", tui.data.serial);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Frame no.:"),
			"Frame no.: %d", tui.data.seq);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Onboard time:"),
			"Onboard time: %s", tui.data.time);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Shutdown in:"),
			"Shutdown in: %s", tui.data.shutdown_timer);
	start_row++;

	mvwprintw(tui.win, start_row++, start_col - sizeof("Latitude:"),
			"Latitude: %.5f%c", fabs(tui.data.lat), tui.data.lat >= 0 ? 'N' : 'S');
	mvwprintw(tui.win, start_row++, start_col - sizeof("Longitude:"),
			"Longitude: %.5f%c", fabs(tui.data.lon), tui.data.lon >= 0 ? 'E' : 'W');
	mvwprintw(tui.win, start_row++, start_col - sizeof("Altitude:"),
			"Altitude: %.0fm", tui.data.alt);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Speed:"),
			"Speed: %.1fm/s", tui.data.spd);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Heading:"),
			"Heading: %.0f'", tui.data.hdg);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Climb:"),
			"Climb: %+.1fm/s", tui.data.climb);
	start_row++;

	mvwprintw(tui.win, start_row++, start_col - sizeof("Temperature:"),
			"Temperature: %.1f'C", tui.data.temp);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Relative hum.:"),
			"Relative hum.: %.0f%%", tui.data.rh);
	mvwprintw(tui.win, start_row++, start_col - sizeof("Dew point:"),
			"Dew point: %.1f'C", dewpt(tui.data.temp, tui.data.rh));
	mvwprintw(tui.win, start_row++, start_col - sizeof("Pressure:"),
			"Pressure: %.1fhPa", synthetic_pressure);
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
		mvwprintw(win, 1, i * elemWidth + (elemWidth - strlen(_decoder_names[i])) / 2, "%s",_decoder_names[i]);
		if (i == selected) wattroff(win, A_STANDOUT);
	}

	wrefresh(win);
}

static void
init_windows(int rows, int cols)
{
	const int width = 50;
	const int height = INFO_COUNT + 3 + 3;
	tui.win = newwin(height, width, (rows - height) / 2, (cols - width) / 2);
	tui.tabs = newwin(2, width, (rows - height) / 2 - 2, (cols - width) / 2);
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
