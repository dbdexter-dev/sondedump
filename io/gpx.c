#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "gpx.h"
#include "utils.h"

#define GPX_TIME_FORMAT "%Y-%m-%dT%H:%M:%SZ"

int
gpx_init(GPXFile *file, char *fname)
{
	file->fd = fopen(fname, "wb");
	file->cur_serial = NULL;

	if (!file->fd) return 1;

	fprintf(file->fd,
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
			"<gpx xmlns=\"http://www.topografix.com/GPX/1/1\" version=\"1.1\" creator=\"SondeDump\">\n"

			);

	file->offset = ftell(file->fd);
	fprintf(file->fd, "</gpx>\n");

	return 0;
}

void
gpx_close(GPXFile *file)
{
	if (file->cur_serial) gpx_stop_track(file);

	if (file && file->fd) {
		fseek(file->fd, file->offset, SEEK_SET);
		fprintf(file->fd, "</gpx>\n");
		fclose(file->fd);
	}
}

void
gpx_start_track(GPXFile *file, char *name)
{
	int i;

	if (name[0] == 0) return;
	for (i=0; name[i] != 0; i++) {
		if (!isalnum(name[i])) return;
	}

	/* If track already open, return immediately */
	if (file->cur_serial != NULL && !strcmp(name, file->cur_serial)) return;

	/* If track open but name doesn't match, stop the track first */
	if (file->cur_serial != NULL) gpx_stop_track(file);
	file->cur_serial = my_strdup(name);

	fseek(file->fd, file->offset, SEEK_SET);
	fprintf(file->fd, "<trk>\n");
	fprintf(file->fd, "<name>%s</name>\n", name);
	fprintf(file->fd, "<trkseg>\n");
	file->offset = ftell(file->fd);

	/* Temporarily terminate file */
	fprintf(file->fd, "</trkseg>\n</trk>\n</gpx>\n");
	fflush(file->fd);
}

void
gpx_add_trackpoint(GPXFile *file, float lat, float lon, float alt, float spd, float hdg, time_t time)
{
	if (!file->cur_serial) return;
	char timestr[sizeof("YYYY-MM-DDThh:mm:ssZ")+1];

	/* Prevent NaNs from breaking the GPX specification */
	if (isnan(lat) || isnan(lon) || isnan(alt)) return;
	if (lat == 0 && lon == 0 && alt == 0) return;

	strftime(timestr, sizeof(timestr), GPX_TIME_FORMAT, gmtime(&time));

	/* Add trackpoint */
	fseek(file->fd, file->offset, SEEK_SET);
	fprintf(file->fd, "<trkpt lat=\"%f\" lon=\"%f\">\n", lat, lon);
	fprintf(file->fd, "<time>%s</time>\n", timestr);
	fprintf(file->fd, "<ele>%f</ele>\n", alt);
	fprintf(file->fd, "<speed>%f</speed>\n", spd);
	fprintf(file->fd, "<course>%f</course>\n", hdg);
	fprintf(file->fd, "</trkpt>\n");
	file->offset = ftell(file->fd);

	/* Temporarily terminate file */
	fprintf(file->fd, "</trkseg>\n</trk>\n</gpx>\n");
	fflush(file->fd);
}

void
gpx_stop_track(GPXFile *file)
{
	fseek(file->fd, file->offset, SEEK_SET);
	fprintf(file->fd, "</trkseg>\n</trk>\n");
	file->offset = ftell(file->fd);

	fprintf(file->fd, "</gpx>\n");

	free(file->cur_serial);
	file->cur_serial = NULL;
}
