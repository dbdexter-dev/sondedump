#include <stdio.h>
#include <cairo/cairo.h>
#include <math.h>
#include "stuve.h"
#include "utils.h"

#define BACKDROP_GRAY 0.1
#define PADDING_PERCENT 0.05
#define FONT_SIZE 10
#define PADDING_W(width) (width * PADDING_PERCENT)
#define PADDING_H(height) (height * PADDING_PERCENT)

static int stuve_draw_isotherms(cairo_t *cr, float w, float h, float pw, float ph, float min, float max, float step);
static int stuve_draw_isobars(cairo_t *cr, float w, float h, float pw, float ph, float min, float max, float step);
static int stuve_draw_mix_ratios(cairo_t *cr, float w, float h, float pw, float ph, float t_min, float t_max, float p_min, float p_max);
static int stuve_draw_dry_adiabats(cairo_t *cr, float w, float h, float pw, float ph, float t_min, float t_max, float p_min, float p_max);
static int stuve_draw_saturated_adiabats(cairo_t *cr, float w, float h, float pw, float ph, float t_min, float t_max, float p_min, float p_max);

int
stuve_draw_backdrop(cairo_surface_t *surface, float t_min, float t_max, float p_min, float p_max)
{
	const double mix_ratios_dash[] = {5, 5};
	const float width = cairo_image_surface_get_width(surface);
	const float height = cairo_image_surface_get_height(surface);
	const float padding_w = PADDING_W(width);
	const float padding_h = PADDING_H(height);
	cairo_t *cr;
	cairo_font_options_t *fontopts;

	cr = cairo_create(surface);

	/* Draw backdrop TODO change/delete? */
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);

	/* Choose font for axes labels */
	cairo_select_font_face(cr, "cairo:monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, FONT_SIZE);
	fontopts = cairo_font_options_create();
	cairo_get_font_options(cr, fontopts);
	cairo_font_options_set_antialias(fontopts, CAIRO_ANTIALIAS_NONE);
	cairo_set_font_options(cr, fontopts);

	/* Draw adiabats indicator lines */
	cairo_set_line_width(cr, 0.5);
	cairo_set_source_rgb(cr, BACKDROP_GRAY, BACKDROP_GRAY, BACKDROP_GRAY);
	cairo_set_dash(cr, NULL, 0, 0);
	stuve_draw_dry_adiabats(cr, width, height, padding_w, padding_h, t_min, t_max, p_min, p_max);
	cairo_set_source_rgb(cr, 0.1, 0.4, 0.1);
	stuve_draw_saturated_adiabats(cr, width, height, padding_w, padding_h, t_min, t_max, p_min, p_max);

	/* Draw mixing ratios */
	cairo_set_source_rgb(cr, 0.5, 0.7, 0.0);
	cairo_set_dash(cr, mix_ratios_dash, LEN(mix_ratios_dash), 0);
	stuve_draw_mix_ratios(cr, width, height, padding_w, padding_h, t_min, t_max, p_min, p_max);

	/* Draw border */
	cairo_set_source_rgb(cr, BACKDROP_GRAY, BACKDROP_GRAY, BACKDROP_GRAY);
	cairo_set_dash(cr, NULL, 0, 0);
	cairo_rectangle(cr, padding_w, padding_h, width-2*padding_w, height-2*padding_h);
	cairo_stroke(cr);

	/* Draw xy grid */
	cairo_set_source_rgb(cr, BACKDROP_GRAY, BACKDROP_GRAY, BACKDROP_GRAY);
	cairo_set_dash(cr, NULL, 0, 0);
	stuve_draw_isotherms(cr, width, height, padding_w, padding_h, t_min, t_max, STUVE_ISOTHERM_TICKSTEP);
	stuve_draw_isobars(cr, width, height, padding_w, padding_h, p_min, p_max, STUVE_ISOBAR_TICKSTEP);


	cairo_destroy(cr);

	return 0;
}
int
stuve_draw_point(cairo_surface_t *surface, float t_min, float t_max, float p_min, float p_max, float temp, float press, float dewpt)
{
	const float width = cairo_image_surface_get_width(surface);
	const float height = cairo_image_surface_get_height(surface);
	const float padding_w = PADDING_W(width);
	const float padding_h = PADDING_H(height);
	const float scale_w = width - 2*padding_w;
	const float scale_h = height - 2*padding_h;
	float x_percent, y_percent, x, y;
	cairo_t *cr;

	if (press != press || press < 0) return 1;
	if (p_min <= 0 || p_max <= 0) return 1;
	y_percent = (logf(press) - logf(p_min)) / (logf(p_max) - logf(p_min));
	y = MAX(0, MIN(1, y_percent)) * scale_h + padding_h;

	cr = cairo_create(surface);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);


	/* Draw temperature if not NAN */
	if (temp == temp) {
		cairo_set_source_rgb(cr, 1, 0, 0);
		x_percent = ((temp - t_min) / (t_max - t_min));

		x = MAX(0, MIN(1, x_percent)) * scale_w + padding_w;
		cairo_move_to(cr, x, y);
		cairo_line_to(cr, x, y);
		cairo_stroke(cr);
	}

	/* Draw dew point if not NAN */
	if (dewpt == dewpt) {
		cairo_set_source_rgb(cr, 0, 0.2, 0.8);
		x_percent = (dewpt - t_min) / (t_max - t_min);
		x = MAX(0, MIN(1, x_percent)) * scale_w + padding_w;
		cairo_move_to(cr, x, y);
		cairo_line_to(cr, x, y);
		cairo_stroke(cr);
	}

	cairo_destroy(cr);
	return 0;
}

/* Static functions {{{ */
static int
stuve_draw_isotherms(cairo_t *cr, float w, float h, float pw, float ph, float min, float max, float step)
{
	const float scale = (w - 2*pw) / (max - min);
	float cur, coord;
	char label[8];

	label[7] = 0;
	for (cur=min + fmod(min, step); cur<=max; cur+=step) {
		coord = (cur - min) * scale;

		cairo_move_to(cr, coord+pw, ph);
		cairo_line_to(cr, coord+pw, h-ph);

		snprintf(label, LEN(label)-1, "%.0f", cur);
		cairo_move_to(cr, (int)(coord+pw)-FONT_SIZE, (int)(h-ph+FONT_SIZE+2));
		cairo_show_text(cr, label);
	}
	cairo_stroke(cr);

	return 0;
}

static int
stuve_draw_isobars(cairo_t *cr, float w, float h, float pw, float ph, float min, float max, float step)
{
	const float scale = (h - 2*ph) / (logf(max) - logf(min));
	float cur, coord;
	char label[8];

	label[7] = 0;
	for (cur=min + fmod(min, step); cur<max; cur+=step) {
		coord = (logf(cur) - logf(min)) * scale;

		snprintf(label, LEN(label)-1, "%.0f", cur);
		cairo_move_to(cr, 0, (int)(coord+ph+FONT_SIZE/2));
		cairo_show_text(cr, label);

		cairo_move_to(cr, pw, coord+ph);
		cairo_line_to(cr, w-pw, coord+ph);

	}
	cairo_stroke(cr);


	return 0;
}

static int
stuve_draw_mix_ratios(cairo_t *cr, float w, float h, float pw, float ph, float t_min, float t_max, float p_min, float p_max)
{
	const float ratios[] = {40, 20, 10, 5, 3, 2, 1, .6f, .2f, .1f};
	const float temps[] = {36.365, 24.76, 13.86, 3.72, -3.27, -8.55, -17.1, -23, -34.5, -41};   /* Precomputed for p_max = 1000 TODO formula-based */
	const float xscale = (w - 2*pw);
	const float yscale = (h - 2*ph);
	float gamma_m = -0.002;         /* Mixing ratio lapse rate (TODO more rigorous calculation?) */
	float start_p, start_t, end_p, end_t;
	float start_p_percent, start_t_percent, end_p_percent, end_t_percent;
	float mixing_ratio;
	char label[8];
	size_t i;

	start_p = p_max;
	start_p_percent = (logf(start_p) - logf(p_min)) / (logf(p_max) - logf(p_min));
	for (i=0; i<LEN(ratios); i++) {
		mixing_ratio = ratios[i];
		start_t = temps[i];
		start_t_percent = (start_t - t_min) / (t_max - t_min);

		snprintf(label, LEN(label), "%.1f", mixing_ratio);
		cairo_move_to(cr, start_t_percent * xscale + pw, start_p_percent*yscale+ph+2*FONT_SIZE);
		cairo_show_text(cr, label);

		cairo_move_to(cr, start_t_percent * xscale + pw, start_p_percent * yscale + ph);

		end_p = p_min;
		end_t = start_t + (pressure_to_altitude(end_p) - pressure_to_altitude(start_p)) * gamma_m;
		end_t_percent = (end_t - t_min) / (t_max - t_min);
		end_p_percent = (logf(end_p) - logf(p_min)) / (logf(p_max) - logf(p_min));
		cairo_line_to(cr, end_t_percent * xscale + pw, end_p_percent * yscale + ph);

	}
	cairo_stroke(cr);
	return 0;
}

static int
stuve_draw_dry_adiabats(cairo_t *cr, float w, float h, float pw, float ph, float t_min, float t_max, float p_min, float p_max)
{
	const float xscale = (w - 2*pw);
	const float yscale = (h - 2*ph);
	const float c_p = 1005;             /* Dry air specific isobar specific heat */
	const float g = 9.81;
	const float gamma_d = -g/c_p;        /* Dry adiabatic lapse rate, dT/dz */
	float start_p, start_t, end_p, end_t;
	float start_p_percent, start_t_percent, end_p_percent, end_t_percent;

	t_min += 273.15;
	t_max += 273.15;

	start_p = p_max;
	start_p_percent = (logf(start_p) - logf(p_min)) / (logf(p_max) - logf(p_min));
	for (start_t = t_min; start_t <= t_max; start_t += STUVE_ISOTHERM_TICKSTEP) {
		start_t_percent = (start_t - t_min) / (t_max - t_min);
		cairo_move_to(cr, start_t_percent * xscale + pw, start_p_percent * yscale + ph);

		end_t = t_min;
		end_p = altitude_to_pressure(pressure_to_altitude(start_p) + (end_t - start_t) / gamma_d);
		end_t_percent = (end_t - t_min) / (t_max - t_min);
		end_p_percent = (logf(end_p) - logf(p_min)) / (logf(p_max) - logf(p_min));
		cairo_line_to(cr, end_t_percent * xscale + pw, end_p_percent * yscale + ph);
	}
	cairo_stroke(cr);
	return 0;
}

static int
stuve_draw_saturated_adiabats(cairo_t *cr, float w, float h, float pw, float ph, float t_min, float t_max, float p_min, float p_max)
{
	const float xscale = (w - 2*pw);
	const float yscale = (h - 2*ph);
	const float c_pd = 1005;            /* Dry air specific isobar specific heat */
	const float g = 9.81;
	const float h_vap = 2.501e6;        /* Heat of vaporization of water */
	const float r_sd = 287;             /* Specific gas constant of dry air */
	const float r_sw = 461.5;           /* Specific gas constant of water vapour */
	float gamma_w;
	float ratio;
	float start_p, start_t, tmp_t, tmp_p, end_p, end_t;
	float start_p_percent, start_t_percent, end_p_percent, end_t_percent;

	t_min += 273.15;
	t_max += 273.15;

	start_p = p_max;
	start_p_percent = (logf(start_p) - logf(p_min)) / (logf(p_max) - logf(p_min));
	for (start_t = t_min + STUVE_ISOTHERM_TICKSTEP/2; start_t <= t_max; start_t += STUVE_ISOTHERM_TICKSTEP) {
		start_t_percent = (start_t - t_min) / (t_max - t_min);

		cairo_move_to(cr, start_t_percent * xscale + pw, start_p_percent * yscale + ph);

		end_t = t_min;
		tmp_p = start_p;
		tmp_t = start_t;
		for (end_t = start_t; end_t >= t_min; end_t -= 1) {
			/* Update saturation mixing ratio for the current temperature and
			 * pressure */
			ratio = 1e-2 * sat_mixing_ratio(tmp_t-273.15, tmp_p);
			/* Compute new wet adiabatic lapse rate */
			gamma_w = -g *
				(1 + (h_vap * ratio) / (r_sd*tmp_t)) /
				(c_pd + (h_vap*h_vap*ratio)/(r_sw*start_t*tmp_t));


			end_p = altitude_to_pressure(pressure_to_altitude(tmp_p) + (end_t - tmp_t) / gamma_w);
			end_t_percent = (end_t - t_min) / (t_max - t_min);
			end_p_percent = (logf(end_p) - logf(p_min)) / (logf(p_max) - logf(p_min));

			cairo_line_to(cr,
					MAX(pw, end_t_percent * xscale + pw),
					MAX(ph, end_p_percent * yscale + ph));

			tmp_t = end_t;
			tmp_p = end_p;
		}
	}

	cairo_stroke(cr);
	return 0;

	return 0;
}
/* }}} */
