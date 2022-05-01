#include <include/dfm09.h>
#include <include/imet4.h>
#include <include/ims100.h>
#include <include/m10.h>
#include <include/rs41.h>
#include <math.h>
#include <string.h>
#include "decode.h"
#include "physics.h"
#include "utils.h"

static void fill_printable_data(PrintableData *to_print, SondeData *data);

static ParserStatus (*active_decoder_decode)(void*, SondeData*, const float*, size_t);
static void *active_decoder_ctx;
static enum decoder active_decoder;
static int decoder_changed;

static RS41Decoder *rs41decoder;
static DFM09Decoder *dfm09decoder;
static IMS100Decoder *ims100decoder;
static M10Decoder *m10decoder;
static IMET4Decoder *imet4decoder;

static PrintableData printable[2];
static int printable_active_slot;
static int has_data, new_data;

void
decoder_init(int samplerate)
{
	/* Initialize decoders */
	rs41decoder = rs41_decoder_init(samplerate);
	dfm09decoder = dfm09_decoder_init(samplerate);
	ims100decoder = ims100_decoder_init(samplerate);
	m10decoder = m10_decoder_init(samplerate);
	imet4decoder = imet4_decoder_init(samplerate);

	/* Initialize pointers to "no decoder" */
	active_decoder_decode = NULL;
	active_decoder_ctx = NULL;
	active_decoder = AUTO;

	/* Initialize data double-buffer */
	memset(printable, 0, sizeof(printable));
	printable_active_slot = 1;
	has_data = 0;
	new_data = 0;

	decoder_changed = 1;
}

void
decoder_deinit(void)
{
	/* Deinitialize all decoders */
	rs41_decoder_deinit(rs41decoder);
	ims100_decoder_deinit(ims100decoder);
	m10_decoder_deinit(m10decoder);
	dfm09_decoder_deinit(dfm09decoder);
	imet4_decoder_deinit(imet4decoder);
}


ParserStatus
decode(const float *srcbuf, size_t len)
{
	SondeData data;

	data.type = EMPTY;

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
			default:
				break;
		}

		decoder_changed = 0;
	}

	/* Parse based on decoder */
	switch (active_decoder) {
		case AUTO:
			while (rs41_decode(rs41decoder, &data, srcbuf, len) != PROCEED) {
				if (data.type != EMPTY && data.type != FRAME_END) {
					set_active_decoder(RS41);
				}
			}
			while (m10_decode(m10decoder, &data, srcbuf, len) != PROCEED) {
				if (data.type != EMPTY && data.type != FRAME_END) {
					set_active_decoder(M10);
				}
			}
			while (ims100_decode(ims100decoder, &data, srcbuf, len) != PROCEED) {
				if (data.type != EMPTY && data.type != FRAME_END) {
					set_active_decoder(IMS100);
				}
			}
			while (dfm09_decode(dfm09decoder, &data, srcbuf, len) != PROCEED) {
				if (data.type != EMPTY && data.type != FRAME_END) {
					set_active_decoder(DFM09);
				}
			}
			while (imet4_decode(imet4decoder, &data, srcbuf, len) != PROCEED) {
				if (data.type != EMPTY && data.type != FRAME_END) {
					set_active_decoder(IMET4);
				}
			}
			break;
		case END:
			/* Decoder is being changed: wait */
			return PARSED;
			break;
		default:
			while (active_decoder_decode(active_decoder_ctx, &data, srcbuf, len) != PROCEED) {
				fill_printable_data(&printable[printable_active_slot], &data);

				switch (data.type) {
					case FRAME_END:
						/* End of printable data */
						new_data = has_data;
						if (has_data) {
							/* Fudge the altitude if it's invalid */
							if (!isnormal(printable[printable_active_slot].pressure)
							 || printable[printable_active_slot].pressure < 0) {
								printable[printable_active_slot].pressure
									= altitude_to_pressure(printable[printable_active_slot].alt);
							}

							/* Swap buffers */
							printable[(printable_active_slot + 1) % LEN(printable)] = printable[printable_active_slot];
							printable_active_slot = (printable_active_slot + 1) % LEN(printable);
							has_data = 0;
						}
						return PARSED;
					case EMPTY:
						break;
					default:
						has_data = 1;
						break;
				}
			}
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
	/* Signal that pointers are okay again */
	active_decoder = (decoder + END) % END;
	memset(printable, 0, sizeof(printable));
	decoder_changed = 1;
}

PrintableData*
get_data(void)
{
	const int idx = (printable_active_slot + 1) % LEN(printable);
	return &printable[idx];
}

int
get_slot(void)
{
	return printable_active_slot;
}

static void
fill_printable_data(PrintableData *to_print, SondeData *data)
{
	switch (data->type) {
		case EMPTY:
		case FRAME_END:
		case UNKNOWN:
		case SOURCE_END:
			break;
		case DATETIME:
			to_print->utc_time = data->data.datetime.datetime;
			break;
		case INFO:
			strncpy(to_print->serial, data->data.info.sonde_serial, LEN(to_print->serial)-1);
			to_print->seq = data->data.info.seq;
			to_print->shutdown_timer = data->data.info.burstkill_status;
			break;
		case PTU:
			to_print->temp = data->data.ptu.temp;
			to_print->rh = data->data.ptu.rh;
			to_print->pressure  = data->data.ptu.pressure;
			to_print->calibrated = data->data.ptu.calibrated;
			to_print->calib_percent = data->data.ptu.calib_percent;
			break;
		case POSITION:
			to_print->lat = data->data.pos.lat;
			to_print->lon = data->data.pos.lon;
			to_print->alt = data->data.pos.alt;
			to_print->speed = data->data.pos.speed;
			to_print->heading = data->data.pos.heading;
			to_print->climb = data->data.pos.climb;
			break;
		case XDATA:
			strcpy(to_print->xdata, data->data.xdata.data);
			break;
	}
}

