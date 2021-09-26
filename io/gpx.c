#include "gpx.h"

#define GPX_TIME_FORMAT "%Y-%m-%dT%H:%M:%SZ"

int
gpx_init(GPXFile *file, char *fname)
{
	file->fd = fopen(fname, "wb");
	file->track_active = 0;

	fprintf(file->fd,
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
			"<gpx xmlns=\"http://www.topografix.com/GPX/1/1\" creator=\"SondeDump\">\n"

			);

	return file->fd == NULL ? 0 : 1;
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
	if (file->track_active) gpx_stop_track(file);

	fprintf(file->fd, "<trk>\n");
	fprintf(file->fd, "<name>%s</name>\n", name);
	fprintf(file->fd, "<trkseg>\n");
	file->track_active = 1;
}

void
gpx_add_trackpoint(GPXFile *file, float lat, float lon, float alt, time_t time)
{
	char timestr[sizeof("YYYY-MM-DDThh:mm:ssZ")+1];

	/* Prevent NaNs from breaking the GPX specification */
	if (lat != lat || lon != lon || alt != alt) return;

	strftime(timestr, sizeof(timestr), GPX_TIME_FORMAT, gmtime(&time));

	fprintf(file->fd, "<trkpt lat=\"%f\" lon=\"%f\">\n", lat, lon);
	fprintf(file->fd, "<ele>%f</ele>\n", alt);
	fprintf(file->fd, "<time>%s</time>\n", timestr);
	fprintf(file->fd, "</trkpt>\n");
}

void
gpx_stop_track(GPXFile *file)
{
	fprintf(file->fd, "</trkseg>\n");
	fprintf(file->fd, "</trk>\n");
	file->track_active = 0;
}

