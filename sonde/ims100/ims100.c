#include <include/ims100.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "decode/framer.h"
#include "decode/manchester.h"
#include "frame.h"
#include "protocol.h"
#include "subframe.h"
#include "utils.h"

#ifndef NDEBUG
static FILE *debug, *debug_odd;
#endif

struct ims100decoder {
	GFSKDemod gfsk;
	Correlator correlator;
	RSDecoder rs;
	IMS100ECCFrame raw_frame[4];
	IMS100Frame frame;
	IMS100Calibration calib;
	uint64_t calib_bitmask;
	int state;
	char serial[16];

	struct {
		float alt;
		time_t time;
	} prev_alt, cur_alt;
	IMS100FrameADC adc;
};

enum { READ, PARSE_INFO, PARSE_PTU,
	PARSE_GPS_TIME, PARSE_GPS_POS,
	PARSE_META_FRAGMENTS,
	PARSE_END };

static void update_calibration(IMS100Decoder *self, int seq, uint8_t *fragment);

IMS100Decoder*
ims100_decoder_init(int samplerate)
{
	IMS100Decoder *d = malloc(sizeof(*d));

	gfsk_init(&d->gfsk, samplerate, IMS100_BAUDRATE);
	correlator_init(&d->correlator, IMS100_SYNCWORD, IMS100_SYNC_LEN);
	bch_init(&d->rs, IMS100_REEDSOLOMON_N, IMS100_REEDSOLOMON_K,
			IMS100_REEDSOLOMON_POLY, ims100_bch_roots, IMS100_REEDSOLOMON_T);

	d->state = READ;
	d->calib_bitmask = 0;
	d->prev_alt.alt = 0;
	d->prev_alt.time = 0;
	strcpy(d->serial, "iMS(xxxxxxxx)");
#ifndef NDEBUG
	debug = fopen("/tmp/ims100frames.data", "wb");
	debug_odd = fopen("/tmp/ims100frames_odd.data", "wb");
#endif

	return d;
}

void
ims100_decoder_deinit(IMS100Decoder *d)
{
	gfsk_deinit(&d->gfsk);
	free(d);
#ifndef NDEBUG
	fclose(debug);
	fclose(debug_odd);
#endif
}

SondeData
ims100_decode(IMS100Decoder *self, int (*read)(float *dst))
{
	SondeData data = {.type = EMPTY};
	uint8_t *const raw_frame = (uint8_t*)self->raw_frame;
	unsigned int seq;
	uint32_t validmask;

	switch (self->state) {
		case READ:
			/* Read a new frame */
			if (read_frame_gfsk(&self->gfsk, &self->correlator, raw_frame, read, IMS100_FRAME_LEN, 0) < 0) {
				data.type = SOURCE_END;
				return data;
			}

			/* Decode bits and move them in the right palce */
			manchester_decode(raw_frame, raw_frame, IMS100_FRAME_LEN);
			ims100_frame_descramble(self->raw_frame);

			/* Error correct and remove all ECC bits */
			if (ims100_frame_error_correct(self->raw_frame, &self->rs) < 0) return data;
			ims100_frame_unpack(&self->frame, self->raw_frame);

			__attribute__((fallthrough));
		case PARSE_INFO:
			/* Copy calibration data */
			if (IMS100_DATA_VALID(self->frame.valid, IMS100_MASK_SEQ | IMS100_MASK_CALIB)) {
				seq = ims100_frame_seq(&self->frame);
				update_calibration(self, seq, self->frame.calib);
			}

			data.type = INFO;

			/* Invalidate data if subframe is marked corrupted */
			validmask = IMS100_MASK_SEQ;
			if (!IMS100_DATA_VALID(self->frame.valid, validmask)) data.type = EMPTY;
			if (IMS100_DATA_VALID(self->calib_bitmask, IMS100_CALIB_SERIAL_MASK)) {
				sprintf(self->serial, "iMS%d", (int)ieee754_be(self->calib.serial));
			}

			if (data.type != EMPTY) {
				data.data.info.seq = ims100_frame_seq(&self->frame);
				data.data.info.sonde_serial = self->serial;
			}

			self->state = PARSE_PTU;

#ifndef NDEBUG
			fwrite(&self->calib, sizeof(self->calib), 1, debug);
			fflush(debug);
#endif


			break;

		case PARSE_PTU:
			data.type = PTU;

			/* Invalidate data if subframe is marked corrupted */
			validmask = IMS100_MASK_PTU;
			if (!IMS100_DATA_VALID(self->frame.valid, validmask)) data.type = EMPTY;

			if (data.type != EMPTY) {
				/* Fetch the ADC data carried by this frame based on its seq nr */
				switch (ims100_frame_seq(&self->frame) & 0x3) {
					case 0x00:
						self->adc.ref = (uint16_t)self->frame.adc_val0[0] << 8 | self->frame.adc_val0[1];
						self->adc.temp = (uint16_t)self->frame.adc_val1[0] << 8 | self->frame.adc_val1[1];
						self->adc.rh = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
						break;
					case 0x01:
					case 0x02:
						self->adc.temp = (uint16_t)self->frame.adc_val1[0] << 8 | self->frame.adc_val1[1];
						self->adc.rh = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
						break;
					case 0x03:
						self->adc.rh_temp = (uint16_t)self->frame.adc_val0[0] << 8 | self->frame.adc_val0[1];
						self->adc.temp = (uint16_t)self->frame.adc_val1[0] << 8 | self->frame.adc_val1[1];
						self->adc.ref = (uint16_t)self->frame.adc_val2[0] << 8 | self->frame.adc_val2[1];
						break;

				}

				/* Parse PTU data */
				data.data.ptu.calib_percent = 100.0 * count_ones((uint8_t*)&self->calib_bitmask, sizeof(self->calib_bitmask)) / IMS100_CALIB_FRAGCOUNT;
				data.data.ptu.calibrated = IMS100_DATA_VALID(self->calib_bitmask, IMS100_CALIB_PTU_MASK);
				data.data.ptu.temp = ims100_frame_temp(&self->adc, &self->calib);
				data.data.ptu.rh = ims100_frame_rh(&self->adc, &self->calib);
			}

			switch (ims100_frame_subtype(&self->frame)) {
				case IMS100_SUBTYPE_GPS:
					self->state = PARSE_GPS_TIME;
					break;
				case IMS100_SUBTYPE_META:
					self->state = PARSE_META_FRAGMENTS;
					break;
				default:
					self->state = PARSE_END;
					break;
			}
			break;

		/* GPS subframe parsing {{{ */
		case PARSE_GPS_TIME:
			data.type = DATETIME;

			/* Invalidate data if subframe is marked corrupted */
			validmask = IMS100_GPS_MASK_TIME | IMS100_GPS_MASK_DATE;
			if (!IMS100_DATA_VALID(self->frame.valid, validmask)) data.type = EMPTY;

			if (data.type != EMPTY) {
				data.data.datetime.datetime = ims100_subframe_time(&self->frame.data.gps);
				self->cur_alt.time = data.data.datetime.datetime;
			}

			self->state = PARSE_GPS_POS;
			break;
		case PARSE_GPS_POS:
			data.type = POSITION;

			/* Invalidate data if subframe is marked corrupted */
			validmask = IMS100_GPS_MASK_LAT | IMS100_GPS_MASK_LON | IMS100_GPS_MASK_ALT
			          | IMS100_GPS_MASK_SPEED | IMS100_GPS_MASK_HEADING;
			if (!IMS100_DATA_VALID(self->frame.valid, validmask)) data.type = EMPTY;

			if (data.type != EMPTY) {
				data.data.pos.lat = ims100_subframe_lat(&self->frame.data.gps);
				data.data.pos.lon = ims100_subframe_lon(&self->frame.data.gps);
				data.data.pos.alt = ims100_subframe_alt(&self->frame.data.gps);
				data.data.pos.speed = ims100_subframe_speed(&self->frame.data.gps);
				data.data.pos.heading = ims100_subframe_heading(&self->frame.data.gps);
				self->cur_alt.alt = data.data.pos.alt;

				/* Derive climb rate from altitude */
				if (self->cur_alt.time > self->prev_alt.time) {
					data.data.pos.climb = (self->cur_alt.alt - self->prev_alt.alt)
										/ (self->cur_alt.time - self->prev_alt.time);
				}
				self->prev_alt = self->cur_alt;
			}

			self->state = PARSE_END;
			break;
		/* }}} */
		/* Other subframe parsing (meta? idk) {{{*/
		case PARSE_META_FRAGMENTS:
			self->state = PARSE_END;
			break;
		/* }}} */

		case PARSE_END:
			data.type = FRAME_END;
			self->state = READ;
			break;
		default:
			self->state = READ;
			break;
	}

	return data;
}

static void
update_calibration(IMS100Decoder *self, int seq, uint8_t *fragment)
{
	const int calib_offset = seq % IMS100_CALIB_FRAGCOUNT;
	memcpy(((uint8_t*)&self->calib) + IMS100_CALIB_FRAGSIZE * calib_offset, fragment, IMS100_CALIB_FRAGSIZE);

	self->calib_bitmask |= (1ULL << (63 - calib_offset));
}
