#include <stdlib.h>
#include <string.h>
#include "kml.h"

static void kml_add_point(KMLFile *kml, float lat, float lon, float alt);
static void kml_temp_close(KMLFile *kml);

int
kml_init(KMLFile *kml, char *fname, int live_update)
{
	FILE *live_fd;

	kml->fd = fopen(fname, "wb");
	kml->track_active = 0;
	kml->sonde_serial = NULL;
	kml->live_update = live_update;

	if (!kml->fd) return 1;

	fprintf(kml->fd,
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
			"<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
			"<Document>\n"
			"<visibility>1</visibility>\n"
			"<open>1</open>\n"
			"<Style id=\"sondepath\">"
			"<LineStyle><color>FFFF7800</color><width>4</width></LineStyle>"
			"</Style>\n"
			"<Placemark>\n"
			);

	/* Generate a kml pointing to the live feed */
	if (live_update) {
		live_fd = fopen("live.kml", "wb");

		if (!live_fd) {
			kml_close(kml);
			return 2;
		}

		fprintf(live_fd,
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
			"<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
			"<Document>\n"
				"<NetworkLink>\n"
					"<visibility>1</visibility>\n"
					"<open>1</open>\n"
					"<name>SondeDump live feed</name>\n"
					"<Link>"
						"<href>%s</href>\n"
						"<refreshMode>onInterval</refreshMode>"
						"<refreshInterval>%d</refreshInterval>"
					"</Link>\n"
				"</NetworkLink>\n"
			"</Document>\n"
			"</kml>\n",
			fname,
			KML_REFRESH_INTERVAL
			);

		fclose(live_fd);
		kml_temp_close(kml);
	}

	return 0;
}

void
kml_close(KMLFile *kml)
{
	if (!kml->fd) return;
	if (kml->track_active) kml_end_track(kml);
	if (kml->sonde_serial) free(kml->sonde_serial);
	fclose(kml->fd);
}

void
kml_start_track(KMLFile *kml, char *name)
{
	if (kml->live_update) fseek(kml->fd, kml->next_update_fseek, SEEK_SET);
	kml->sonde_serial = strdup(name);
	fprintf(kml->fd,
			"<name>%s</name>"
			"<styleUrl>#sondepath</styleUrl>\n"
			"<LineString>\n"
			"<tessellate>0</tessellate>\n"
			"<coordinates>\n",
			name
		  );
	if (kml->live_update) kml_temp_close(kml);
	kml->track_active = 1;
}

void
kml_add_trackpoint(KMLFile *kml, float lat, float lon, float alt, time_t time)
{
	if (!kml->track_active) return;
	if (lat != lat || lon != lon || lat != lat) return;
	if (kml->live_update) fseek(kml->fd, kml->next_update_fseek, SEEK_SET);
	fprintf(kml->fd, "%f,%f,%f\n", lon, lat, alt);
	kml->lat = lat;
	kml->lon = lon;
	kml->alt = alt;
	if (kml->live_update) kml_temp_close(kml);
}

void
kml_end_track(KMLFile *kml)
{
	if (kml->live_update) fseek(kml->fd, kml->next_update_fseek, SEEK_SET);
	fprintf(kml->fd,
			"</coordinates>"
			"</LineString>\n"
		  );
	kml->track_active = 0;
	if (kml->live_update) kml_temp_close(kml);
}

static void
kml_temp_close(KMLFile *kml)
{
	kml->next_update_fseek = ftell(kml->fd);
	if (kml->track_active) {
		fprintf(kml->fd,
				"</coordinates>"
				"</LineString>\n"
			  );
	}
	fprintf(kml->fd, "</Placemark>\n");
	if (kml->lat >= 0 && kml->lon >= 0 && kml->alt >= 0) {
		kml_add_point(kml, kml->lat, kml->lon, kml->alt);
	}
	fprintf(kml->fd, "</Document>\n</kml>\n");
	fflush(kml->fd);
}

static void
kml_add_point(KMLFile *kml, float lat, float lon, float alt)
{
	fprintf(kml->fd,
			"<Placemark>\n"
			"<name>%s</name>\n"
			"<Point>\n"
			"<altitudeMode>absolute</altitudeMode>\n"
			"<coordinates>%f,%f,%f</coordinates>\n"
			"</Point>\n"
			"</Placemark>",
			kml->sonde_serial, lon, lat, alt
	);
}
