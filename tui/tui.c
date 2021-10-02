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
#define INFO_COUNT (PTU_INFO_COUNT + GPS_INFO_COUNT + SONDE_INFO_COUNT)

static void init_windows(int rows, int cols);
static void redraw();
static void handle_resize();
static void *main_loop(void* args);

static int _update_interval;
static int _running;
static pthread_t _tid;
static struct {
	WINDOW *win;
	struct {
		int seq;
		float temp, rh, pressure;
		float lat, lon, alt, spd, hdg, climb;
		char serial[8+1];
		char time[32];
		char shutdown_timer[16];
		int changed;
	} data;
} tui;


void
tui_init(int update_interval)
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

	tui.data.serial[0] = 0;
	tui.data.time[0] = 0;
	tui.data.shutdown_timer[0] = 0;

	_running = 1;
	tui.data.changed = 1;
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
			ecef_to_lla(&tui.data.lat, &tui.data.lon, &tui.data.alt,
					data->data.pos.x, data->data.pos.y, data->data.pos.z);
			ecef_to_spd_hdg(&tui.data.spd, &tui.data.hdg, &tui.data.climb,
					tui.data.lat, tui.data.lon,
					data->data.pos.dx, data->data.pos.dy, data->data.pos.dz);
			break;

	}
	tui.data.changed = 1;
	return 0;
}

/* Static functions {{{ */
static void*
main_loop(void *args)
{
	while (_running) {
		switch (wgetch(tui.win)) {
			case KEY_RESIZE:
				handle_resize();
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
	const int height = INFO_COUNT + 3 + 2;
	int rows, cols;
	werase(stdscr);
	endwin();

	refresh();
	getmaxyx(stdscr, rows, cols);
	wresize(tui.win, height, width);
	mvwin(tui.win, (rows - height) / 2, (cols - width) / 2);
	redraw();
}
static void
redraw()
{
	int rows, cols;
	int start_row, start_col;
	float synthetic_pressure;

	getmaxyx(tui.win, rows, cols);
	start_row = (rows - INFO_COUNT - 3) / 2;
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

	wrefresh(tui.win);
}

static void
init_windows(int rows, int cols)
{
	const int width = 50;
	const int height = INFO_COUNT + 3 + 2;
	tui.win = newwin(height, width, (rows - height) / 2, (cols - width) / 2);
	wborder(tui.win,
			0, 0, 0, 0,
			0, 0, 0, 0);
	wtimeout(tui.win, _update_interval);
	wrefresh(tui.win);
}

/* }}} */
