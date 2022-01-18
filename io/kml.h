#ifndef kml_h
#define kml_h

#include <stdio.h>
#include <time.h>

#define KML_REFRESH_INTERVAL 5

typedef struct {
	FILE *fd;
	char *sonde_serial;
	float lat, lon, alt;
	int track_active;
	int live_update;
	long next_update_fseek;
} KMLFile;

/**
 * Initialize a KML output file
 *
 * @param file file struct to initialize
 * @param fname path to write file to
 * @param live_update 1 to generate a companion file for live updates
 *                    0 otherwise
 * @return 0 on success, non-zero otherwise
 */
int kml_init(KMLFile *file, const char *fname, int live_update);

/**
 * Close a KML output file
 *
 * @param file file struct to close
 */
void kml_close(KMLFile *file);

/**
 * Start a new KML track object with a given name. Will silently fail if
 * filename is NULL or contains invalid characters
 *
 * @param file file struct to open track in
 * @param name name of the track to use
 */
void kml_start_track(KMLFile *file, const char *name);

/**
 * Add a trackpoint to the currently open track on the given file. Will silently
 * fail if any input data is invalid, or if a track is not currently open in
 * the given file
 *
 * @param file file to append the trackpoint to
 * @param lat latitude of the point, decimal degrees
 * @param lon longitude of the point, decimal degrees
 * @param alt altitude of the point, meters
 */
void kml_add_trackpoint(KMLFile *file, float lat, float lon, float alt);

/**
 * End a previously opened KML track on the file. Will silently fail if no track
 * is currently open.
 *
 * @param file file struct of which the track should be stopped
 */
void kml_stop_track(KMLFile *file);


#endif
