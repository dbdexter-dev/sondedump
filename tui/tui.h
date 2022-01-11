#ifndef tui_h
#define tui_h

#include <include/data.h>

/**
 * Initialize the ncurses text user interface
 *
 * @param update_interval refresh interval, 0 for default
 * @param decoder_changer thread-safe callback function for when the user
 *        changes the active decoder
 * @param active_decoder index of the decoder currently active
 */
void tui_init(int update_interval, void (*decoder_changer)(int index), int active_decoder);

/**
 * Deinitialize the text user interface
 */
void tui_deinit();

/**
 * Update the data displayed by the TUI
 *
 * @param data data to display
 */
int tui_update(PrintableData *data);
#endif

