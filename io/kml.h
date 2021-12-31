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

int kml_init(KMLFile *file, char *fname, int live_update);
void kml_close(KMLFile *file);
void kml_start_track(KMLFile *file, char *name);
void kml_add_trackpoint(KMLFile *file, float lat, float lon, float alt);
void kml_stop_track(KMLFile *file);
void kml_end_track(KMLFile *file);


#endif
