#include <include/c50.h>
#include <include/dfm09.h>
#include <include/imet4.h>
#include <include/ims100.h>
#include <include/m10.h>
#include <include/rs41.h>
#include <math.h>
#include <string.h>
#include "decode.h"
#include "log/log.h"
#include "physics.h"
#include "utils.h"

#define CHUNKSIZE 1024

static ParserStatus (*active_decoder_decode)(void*, SondeData*, const float*, size_t);
static void *active_decoder_ctx;
static enum decoder active_decoder;
static int decoder_changed;

static RS41Decoder *rs41decoder = NULL;
static DFM09Decoder *dfm09decoder = NULL;
static IMS100Decoder *ims100decoder = NULL;
static M10Decoder *m10decoder = NULL;
static IMET4Decoder *imet4decoder = NULL;
static C50Decoder *c50decoder = NULL;

static SondeData printable;
static int has_data, new_data;

static GeoPoint *track;
static int reserved_count;
static int sample_count;
static int new_samplerate = -1;
static uint64_t id_offset;

int
decoder_init(int samplerate)
{
	if (samplerate <= 0) return 1;

	/* Initialize decoders */
	rs41decoder = rs41_decoder_init(samplerate);
	dfm09decoder = dfm09_decoder_init(samplerate);
	ims100decoder = ims100_decoder_init(samplerate);
	m10decoder = m10_decoder_init(samplerate);
	imet4decoder = imet4_decoder_init(samplerate);
	c50decoder = c50_decoder_init(samplerate);

	/* Initialize pointers to "no decoder" */
	active_decoder_decode = NULL;
	active_decoder_ctx = NULL;
	active_decoder = AUTO;

	/* Initialize historical data pointers */
	track = malloc(CHUNKSIZE * sizeof(*track));
	reserved_count = CHUNKSIZE;
	sample_count = 0;

	id_offset = time(NULL);

	/* Initialize data double-buffer */
	memset(&printable, 0, sizeof(printable));
	has_data = 0;
	new_data = 0;

	decoder_changed = 1;

	return 0;
}


void
decoder_deinit(void)
{
	/* Deinitialize all decoders */
	if (rs41decoder) {
		rs41_decoder_deinit(rs41decoder);
		rs41decoder = NULL;
	}
	if (ims100decoder) {
		ims100_decoder_deinit(ims100decoder);
		ims100decoder = NULL;
	}
	if (m10decoder) {
		m10_decoder_deinit(m10decoder);
		m10decoder = NULL;
	}
	if (dfm09decoder) {
		dfm09_decoder_deinit(dfm09decoder);
		dfm09decoder = NULL;
	}
	if (imet4decoder) {
		imet4_decoder_deinit(imet4decoder);
		imet4decoder = NULL;
	}
	if (c50decoder) {
		c50_decoder_deinit(c50decoder);
		c50decoder = NULL;
	}

	/* Clear history buffers */
	sample_count = 0;
	if (track) {
		free(track);
		track = NULL;
	}
}

void
decoder_set_samplerate(int samplerate)
{
	new_samplerate = samplerate;
}

ParserStatus
decode(const float *srcbuf, size_t len)
{
	SondeData data;

	if (decoder_changed) {
		/* Handle decoder switch */
		switch (active_decoder) {
		case RS41:
			active_decoder_decode = (decoder_iface_t)&rs41_decode;
			active_decoder_ctx = rs41decoder;
			break;
		case DFM09:
			active_decoder_decode = (decoder_iface_t)&dfm09_decode;
			active_decoder_ctx = dfm09decoder;
			break;
		case M10:
			active_decoder_decode = (decoder_iface_t)&m10_decode;
			active_decoder_ctx = m10decoder;
			break;
		case IMS100:
			active_decoder_decode = (decoder_iface_t)&ims100_decode;
			active_decoder_ctx = ims100decoder;
			break;
		case IMET4:
			active_decoder_decode = (decoder_iface_t)&imet4_decode;
			active_decoder_ctx = imet4decoder;
			break;
		case C50:
			active_decoder_decode = (decoder_iface_t)&c50_decode;
			active_decoder_ctx = c50decoder;
			break;
		default:
			break;
		}

		decoder_changed = 0;
	}

	/* Parse based on decoder */
	switch (active_decoder) {
	case AUTO:
		while (rs41_decode(rs41decoder, &data, srcbuf, len) != PROCEED) {
			if (data.fields) {
				log_info("Autodetected: RS41");
				set_active_decoder(RS41);
				break;
			}
		}
		while (m10_decode(m10decoder, &data, srcbuf, len) != PROCEED) {
			if (data.fields) {
				log_info("Autodetected: M10");
				set_active_decoder(M10);
				break;
			}
		}
		while (ims100_decode(ims100decoder, &data, srcbuf, len) != PROCEED) {
			if (data.fields) {
				log_info("Autodetected: iMS100");
				set_active_decoder(IMS100);
				break;
			}
		}
		while (dfm09_decode(dfm09decoder, &data, srcbuf, len) != PROCEED) {
			if (data.fields) {
				log_info("Autodetected: DFM09");
				set_active_decoder(DFM09);
				break;
			}
		}
		while (imet4_decode(imet4decoder, &data, srcbuf, len) != PROCEED) {
			if (data.fields) {
				log_info("Autodetected: iMet-4");
				set_active_decoder(IMET4);
				break;
			}
		}
		while (c50_decode(c50decoder, &data, srcbuf, len) != PROCEED) {
			if (data.fields) {
				log_info("Autodetected: SRS C50");
				set_active_decoder(C50);
				break;
			}
		}
		break;
	case END:
		/* Decoder is being changed: wait */
		return PARSED;
		break;
	default:
		while (active_decoder_decode(active_decoder_ctx, &data, srcbuf, len) != PROCEED) {
			/* If no new data available, return */
			if (!data.fields) continue;

			/* Copy data from previous sample */
			if (sample_count) {
				memcpy(&track[sample_count], &track[sample_count-1], sizeof(track[sample_count]));
			}

			/* Add track point to list {{{ */
			if (data.fields & DATA_PTU) {
				track[sample_count].temp = data.temp;
				track[sample_count].rh = data.rh;
				track[sample_count].pressure = data.pressure;
				track[sample_count].dewpt = dewpt(data.temp, data.rh);
				track[sample_count].pressure = data.pressure;

				printable.fields |= DATA_PTU;
				printable.temp = data.temp;
				printable.rh = data.rh;
				printable.pressure = data.pressure;

				printable.calib_percent = data.calib_percent;
			}

			if (data.fields & DATA_TIME) {
				track[sample_count].utc_time = data.time;

				printable.fields |= DATA_TIME;
				printable.time = data.time;
			}

			if (data.fields & DATA_POS) {
				track[sample_count].lat = data.lat;
				track[sample_count].lon = data.lon;
				track[sample_count].alt = data.alt;

				printable.fields |= DATA_POS;
				printable.lat = data.lat;
				printable.lon = data.lon;
				printable.alt = data.alt;
			}

			if (data.fields & DATA_SPEED) {
				track[sample_count].spd = data.speed;
				track[sample_count].hdg = data.heading;
				track[sample_count].climb = data.climb;

				printable.fields |= DATA_SPEED;
				printable.speed = data.speed;
				printable.heading = data.heading;
				printable.climb = data.climb;
			}

			if (data.fields & DATA_SERIAL) {
				printable.fields |= DATA_SERIAL;
				strncpy(printable.serial, data.serial, sizeof(printable.serial) - 1);
				printable.serial[sizeof(printable.serial) - 1] = 0;
			}

			if (data.fields & DATA_XDATA) {
				printable.fields |= DATA_XDATA;
				printable.xdata = data.xdata;
			}

			if (data.fields & DATA_SHUTDOWN) {
				printable.fields |= DATA_SHUTDOWN;
				printable.shutdown = data.shutdown;
			}

			if (data.fields & DATA_SEQ) {
				/* Generate a unique ID for this packet. Ideally would be the
				 * sequence number, but sondes like the DFM roll over every 256
				 * packets, which is not nearly enough to uniquely identify
				 * every packet sent during a flight */
				track[sample_count].id = MAX((uint32_t)data.seq, sample_count ? track[sample_count - 1].id + 1 : 0);

				printable.fields |= DATA_SEQ;
				printable.seq = data.seq;
			} else if (sample_count) {
				track[sample_count].id = track[sample_count - 1].id + 1;
			} else {
				track[sample_count].id = 0;
			}
			/* }}} */

			/* If pressure data is unavailable, derive it from the reported
			 * altitude */
			if (!(printable.pressure > 0)) {
				printable.pressure = altitude_to_pressure(printable.alt);
				track[sample_count].pressure = printable.pressure;
			}

			sample_count++;
			if (sample_count >= reserved_count) {
				reserved_count += CHUNKSIZE;
				track = realloc(track, reserved_count * sizeof(*track));
			}

			return PARSED;
		}
		break;
	}
	return PROCEED;
}

enum decoder
get_active_decoder(void)
{
	return active_decoder;
}

void
set_active_decoder(enum decoder decoder)
{
	if (decoder == active_decoder) return;

	/* Signal that pointers are okay again */
	active_decoder = (decoder + END) % END;
	memset(&printable, 0, sizeof(printable));
	decoder_changed = 1;
	sample_count = 0;
}

const SondeData*
get_data(void)
{
	return &printable;
}

int
get_slot(void) {
	return sample_count;
}

int
get_data_count(void)
{
	return sample_count;
}

const GeoPoint*
get_track_data(void)
{
	return track;
}

/* Static functions {{{ */
/* }}} */
