#ifndef gpx_h
#define gpx_h

#include <stdio.h>
#include <time.h>

typedef struct {
	FILE *fd;
	size_t offset;
	int track_active;
	char *cur_serial;
} GPXFile;

/**
 * Initialize a GPX output file
 *
 * @param file file struct to initialize
 * @param fname path to write file to
 * @return 0 on success, non-zero otherwise
 */
int gpx_init(GPXFile *file, char *fname);

/**
 * Close a GPX output file
 *
 * @param file file struct to close
 */
void gpx_close(GPXFile *file);

/**
 * Start a new GPX track object with a given name. Will silently fail if
 * filename is NULL or contains invalid characters
 *
 * @param file file struct to open track in
 * @param name name of the track to use
 */
void gpx_start_track(GPXFile *file, char *name);

/**
 * Add a trackpoint to the currently open track on the given file. Will silently
 * fail if any input data is invalid, or if a track is not currently open in
 * the given file
 *
 * @param file file to append the trackpoint to
 * @param lat latitude of the point, decimal degrees
 * @param lon longitude of the point, decimal degrees
 * @param alt altitude of the point, meters
 * @param spd speed of the point, meters/second
 * @param hdg heading of the point, degrees
 * @param time timestamp of the point, UTC
 */
void gpx_add_trackpoint(GPXFile *file, float lat, float lon, float alt, float spd, float hdg, time_t time);

/**
 * End a previously opened GPX track on the file. Will silently fail if no track
 * is currently open.
 *
 * @param file file struct of which the track should be stopped
 */
void gpx_stop_track(GPXFile *file);

#endif
