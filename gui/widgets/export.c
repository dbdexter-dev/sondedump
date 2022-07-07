#include "export.h"
#include "log/log.h"
#include "style.h"
#include "utils.h"
#include "decode.h"
#include "io/gpx.h"
#include "io/kml.h"

static const char *format_names[] = {"GPX", "KML", "CSV"};
static int _selected_idx;
enum format {
	GPX=0,
	KML=1,
	CSV=2
};

void
widget_export(struct nk_context *ctx, float scale)
{
	const GeoPoint *points = get_track_data();
	const int point_count = get_data_count();
	GPXFile gpx;
	KMLFile kml;
	FILE *fd;
	static char fname[512];
	const int label_len = 120;
	struct nk_rect bounds, inner_bounds;
	float border;
	int i;

	border = nk_window_get_panel(ctx)->border;

	nk_layout_row_begin(ctx, NK_STATIC, STYLE_DEFAULT_ROW_HEIGHT * 1.2 * scale, 2);

	/* Latitude text box */
	nk_layout_row_push(ctx, label_len * scale);
	nk_label(ctx, "Filename:", NK_TEXT_LEFT);
	bounds = nk_layout_widget_bounds(ctx);
	nk_layout_row_push(ctx, bounds.w - label_len * scale - 2 * border);
	nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, fname, LEN(fname), NULL);

	nk_layout_row_push(ctx, label_len * scale);
	nk_label(ctx, "Format:", NK_TEXT_LEFT);
	nk_layout_row_push(ctx, bounds.w - label_len * scale - 2 * border);
	bounds = nk_layout_widget_bounds(ctx);
	inner_bounds = nk_widget_bounds(ctx);
	nk_combobox(ctx,
	            format_names,
	            LEN(format_names),
	            &_selected_idx,
	            STYLE_DEFAULT_ROW_HEIGHT * scale,
	            nk_vec2(inner_bounds.w, LEN(format_names) * bounds.h + 10)
	);

	/* TODO exporting in the same thread is not really the best practice, but in
	 * reality it takes so little time that it's probably fine */
	nk_layout_row_dynamic(ctx, STYLE_DEFAULT_ROW_HEIGHT * 1.2 * scale, 1);
	if (nk_button_label(ctx, "Export")) {
		log_debug("Exporting to %s (format %s)", fname, format_names[_selected_idx]);

		switch (_selected_idx) {
		case GPX:
			if (gpx_init(&gpx, fname)) {
				log_error("Failed to export");
				break;
			}

			gpx_start_track(&gpx, get_data()->serial);
			for (i=0; i<point_count; i++) {
				gpx_add_trackpoint(&gpx,
				                    points[i].lat, points[i].lon, points[i].alt,
				                    points[i].spd, points[i].hdg, points[i].utc_time);
			}
			gpx_stop_track(&gpx);
			gpx_close(&gpx);

			log_info("GPX export successful");
			break;
		case KML:
			if (kml_init(&kml, fname, 0)) {
				log_error("Failed to export");
				break;
			}

			kml_start_track(&kml, get_data()->serial);
			for (i=0; i<point_count; i++) {
				kml_add_trackpoint(&kml, points[i].lat, points[i].lon, points[i].alt);
			}
			kml_stop_track(&kml);
			kml_close(&kml);

			log_info("KML export successful");
			break;
		case CSV:
			if (!(fd = fopen(fname, "wb"))) {
				log_error("Failed to export");
				break;
			}

			fprintf(fd, "Temperature,RH,Pressure,Altitude,Latitude,Longitude,Speed,Heading,Climb\n");
			for (i=0; i<point_count; i++) {
				fprintf(fd, "%f,%f,%f,%f,%f,%f,%f,%f,%f\n",
						points[i].temp, points[i].rh, points[i].pressure,
						points[i].alt, points[i].lat, points[i].lon,
						points[i].spd, points[i].hdg, points[i].climb);
			}
			fclose(fd);

			log_info("CSV export successful");

			break;
		}
	}
}
