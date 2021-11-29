#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decode/ecc/crc.h"
#include "decode/prng.h"
#include "frame.h"
#include "gps/ecef.h"
#include "gps/time.h"
#include "rs41.h"
#include "subframe.h"

static void rs41_update_metadata(RS41Metadata *m, RS41Subframe_Status *s);

#ifndef NDEBUG
static FILE *debug;
static FILE *metafile;
#endif

void
rs41_decoder_init(RS41Decoder *d, int samplerate)
{
	correlator_init(&d->correlator, RS41_SYNCWORD, RS41_SYNC_LEN);
	gfsk_init(&d->gfsk, samplerate, RS41_BAUDRATE);
	rs_init(&d->rs, RS41_REEDSOLOMON_N, RS41_REEDSOLOMON_K, RS41_REEDSOLOMON_POLY,
			RS41_REEDSOLOMON_FIRST_ROOT, RS41_REEDSOLOMON_ROOT_SKIP);

	d->state = READ;
	d->metadata.initialized = 0;
	memset(&d->metadata.data, 0, sizeof(d->metadata.data));

#ifndef NDEBUG
	debug = fopen("/tmp/rs41frames.data", "wb");
	metafile = fopen("/tmp/meta.data", "wb");
#endif
}

void
rs41_decoder_deinit(RS41Decoder *d)
{
	gfsk_deinit(&d->gfsk);
	rs_deinit(&d->rs);
#ifndef NDEBUG
	fclose(debug);
	fclose(metafile);
#endif
}

SondeData
rs41_decode(RS41Decoder *self, int (*read)(float *dst))
{
	SondeData data;
	RS41Subframe *subframe;
	int frame_data_len;
	int offset;
	int inverted;
	int i, errors;
	int burstkill_timer;

	RS41Subframe_Status *status;
	RS41Subframe_PTU *ptu;
	RS41Subframe_GPSInfo *gpsinfo;
	RS41Subframe_GPSPos *gpspos;

	switch (self->state) {
		case READ:
			/* Read a frame worth of bits */
			if (!gfsk_demod(&self->gfsk, (uint8_t*)self->frame, 0, RS41_MAX_FRAME_LEN*8, read)) {
				data.type = SOURCE_END;
				return data;
			}

			/* Find the offset with the strongest correlation with the sync marker */
			offset = correlate(&self->correlator, &inverted, (uint8_t*)self->frame, RS41_MAX_FRAME_LEN);
			if (offset) {
				/* Read more bits to compensate for the frame offset */
				if (!gfsk_demod(&self->gfsk, (uint8_t*)self->frame + RS41_MAX_FRAME_LEN, 0, offset, read)) {
					data.type = SOURCE_END;
					return data;
				}
				bitcpy((uint8_t*)self->frame, (uint8_t*)self->frame, offset, 8*RS41_MAX_FRAME_LEN);
			}

			/* Invert if necessary */
			if (inverted) {
				for (i=0; i<RS41_MAX_FRAME_LEN; i++) {
					((uint8_t*)self->frame)[i] ^= 0xFF;
				}
			}

			/* Descramble and error correct */
			rs41_frame_descramble(self->frame);
			errors = rs41_frame_correct(self->frame, &self->rs);

#ifndef NDEBUG
			/* Output the frame to file */
			fwrite(self->frame, RS41_MAX_FRAME_LEN, 1, debug);
#endif

			if (errors < 0) {
				data.type = FRAME_END;
				return data;
			}

			/* Prepare to parse subframes */
			self->offset = 0;
			self->state = PARSE_SUBFRAME;
			__attribute__((fallthrough));

		case PARSE_SUBFRAME:
			/* Parse expected data length from extended flag */
			frame_data_len = RS41_DATA_LEN + (rs41_frame_is_extended(self->frame) ? RS41_XDATA_LEN : 0);

			/* Update pointer to the subframe */
			subframe = (RS41Subframe*)&self->frame->data[self->offset];
			self->offset += subframe->len + 4;

			/* If the frame ends out of bounds, we reached the end: read next */
			if (self->offset >= frame_data_len) {
				data.type = FRAME_END;
				self->state = READ;
				break;
			}

			/* Validate the subframe's checksum against the one received.
			 * If it doesn't match, discard it */
			if (crc16_ccitt_false(subframe->data, subframe->len) != *(uint16_t*)&subframe->data[subframe->len]) {
				data.type = EMPTY;
				break;
			}

			/* Subframe parsing {{{ */
			switch (subframe->type) {
				case RS41_SFTYPE_EMPTY:
					/* Padding */
					data.type = EMPTY;
					break;
				case RS41_SFTYPE_INFO:
					/* Frame sequence number, serial no., board info, calibration data */
					status = (RS41Subframe_Status*)subframe;
					rs41_update_metadata(&self->metadata, (RS41Subframe_Status*)subframe);

					data.type = INFO;

					data.data.info.seq = status->frame_seq;
					data.data.info.sonde_serial = status->serial;
					burstkill_timer = self->metadata.data.burstkill_timer;
					data.data.info.burstkill_status = (burstkill_timer == 0xFFFF ? -1 : burstkill_timer);
					break;
				case RS41_SFTYPE_PTU:
					/* Temperature, humidity, pressure */
					ptu = (RS41Subframe_PTU*)subframe;

					data.type = PTU;

					data.data.ptu.temp = rs41_subframe_temp(ptu, &self->metadata.data);
					data.data.ptu.rh = rs41_subframe_humidity(ptu, &self->metadata.data);
					data.data.ptu.pressure = rs41_subframe_pressure(ptu, &self->metadata.data);
					break;
				case RS41_SFTYPE_GPSPOS:
					/* GPS position */
					gpspos = (RS41Subframe_GPSPos*)subframe;

					data.type = POSITION;

					data.data.pos.x = rs41_subframe_x(gpspos);
					data.data.pos.y = rs41_subframe_y(gpspos);
					data.data.pos.z = rs41_subframe_z(gpspos);
					data.data.pos.dx = rs41_subframe_dx(gpspos);
					data.data.pos.dy = rs41_subframe_dy(gpspos);
					data.data.pos.dz = rs41_subframe_dz(gpspos);
					break;
				case RS41_SFTYPE_GPSINFO:
					/* GPS date/time and RSSI */
					gpsinfo = (RS41Subframe_GPSInfo*)subframe;

					data.type = DATETIME;

					data.data.datetime.datetime = gps_time_to_utc(gpsinfo->week, gpsinfo->ms);
					break;
				default:
					/* Unknown */
					data.type = UNKNOWN;
					break;
			}
			/* }}} */


			break;
		default:
			data.type = UNKNOWN;
			break;
	}

	return data;
}

/* Static functions {{{ */
static void
rs41_update_metadata(RS41Metadata *m, RS41Subframe_Status *s)
{
	size_t frag_offset;
	int num_segments;
	size_t i;

	/* Allocate enough space to contain the misc metadata */
	if (!m->initialized) {
		num_segments = s->frag_count + 1;
		memset(&m->missing, 0xFF, sizeof(m->missing));
		m->missing[sizeof(m->missing)-1] &= ~((1 << (7 - num_segments%8)) - 1);
		m->initialized = 1;
	}

	/* Copy the fragment and update the bitmap of the fragments left */
	frag_offset = s->frag_seq * LEN(s->frag_data);
	memcpy((uint8_t*)&m->data + frag_offset, s->frag_data, LEN(s->frag_data));
	m->missing[s->frag_seq/8] &= ~(1 << s->frag_seq%8);

	/* Check if we have all the sub-segments populated */
	for (i=0; i<sizeof(m->missing); i++) {
		if (m->missing[i]) return;
	}

#ifndef NDEBUG
	fwrite(&m->data, sizeof(m->data), 1, metafile);
#endif
}
/* }}} */
