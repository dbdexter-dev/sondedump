#include <ctype.h>
#include <math.h>
#include "gpx.h"

#define GPX_TIME_FORMAT "%Y-%m-%dT%H:%M:%SZ"

int
gpx_init(GPXFile *file, char *fname)
{
	file->fd = fopen(fname, "wb");
	file->track_active = 0;

	if (!file->fd) return 1;

	fprintf(file->fd,
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
			"<gpx xmlns=\"http://www.topografix.com/GPX/1/1\" version=\"1.1\" creator=\"SondeDump\">\n"

			);

	return 0;
}

void
gpx_close(GPXFile *file)
{
	if (file->track_active) gpx_stop_track(file);

	if (file && file->fd) {
		fprintf(file->fd, "</gpx>\n");
		fclose(file->fd);
	}
}

void
gpx_start_track(GPXFile *file, char *name)
{
	int i;

	for (i=0; name[i] != 0; i++) {
		if (!isalnum(name[i])) return;
	}

	if (file->track_active) gpx_stop_track(file);

	fprintf(file->fd, "<trk>\n");
	fprintf(file->fd, "<name>%s</name>\n", name);
	fprintf(file->fd, "<trkseg>\n");
	file->track_active = 1;
}

void
gpx_add_trackpoint(GPXFile *file, float lat, float lon, float alt, float spd, float hdg, time_t time)
{
	char timestr[sizeof("YYYY-MM-DDThh:mm:ssZ")+1];

	/* Prevent NaNs from breaking the GPX specification */
	if (isnan(lat) || isnan(lon) || isnan(alt)) return;
	if (lat == 0 && lon == 0 && alt == 0) return;

	strftime(timestr, sizeof(timestr), GPX_TIME_FORMAT, gmtime(&time));

	fprintf(file->fd, "<trkpt lat=\"%f\" lon=\"%f\">\n", lat, lon);
	fprintf(file->fd, "<ele>%f</ele>\n", alt);
	fprintf(file->fd, "<time>%s</time>\n", timestr);
	fprintf(file->fd, "<speed>%f</speed>\n", spd);
	fprintf(file->fd, "<course>%f</course>\n", hdg);
	fprintf(file->fd, "</trkpt>\n");
	fflush(file->fd);
}

void
gpx_stop_track(GPXFile *file)
{
	fprintf(file->fd, "</trkseg>\n");
	fprintf(file->fd, "</trk>\n");
	file->track_active = 0;
}

