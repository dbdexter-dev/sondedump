#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decode/prng.h"
#include "ecc/crc.h"
#include "frame.h"
#include "rs41.h"
#include "subframe.h"
#include "gps/ecef.h"

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
	uint8_t tmp;
	int frame_data_len;
	int offset;
	int inverted;
	int i, errors;

	float lat, lon, alt;

	RS41Subframe_Status *status;
	RS41Subframe_PTU *ptu;

	switch (self->state) {
		case READ:
			/* Read a frame worth of bits */
			if (!gfsk_decode(&self->gfsk, (uint8_t*)self->frame, 0, RS41_MAX_FRAME_LEN*8, read)) {
				data.type = SOURCE_END;
				return data;
			}

			/* Find the offset with the strongest correlation with the sync marker */
			offset = correlate(&self->correlator, &inverted, (uint8_t*)self->frame, RS41_MAX_FRAME_LEN);
			if (offset) {
				/* Read more bits to compensate for the frame offset */
				if (!gfsk_decode(&self->gfsk, (uint8_t*)self->frame + RS41_MAX_FRAME_LEN, 0, offset, read)) {
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
			if (errors < 0) {
				data.type = FRAME_END;
				return data;
			}
#ifndef NDEBUG
			/* Output the frame to file */
			fwrite(self->frame, RS41_MAX_FRAME_LEN, 1, debug);
#endif

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

			/* Validate the subframe's checksum. If it doesn't match, discard
			 * the subframe */
			tmp = subframe->data[subframe->len];
			subframe->data[subframe->len] = subframe->data[subframe->len+1];
			subframe->data[subframe->len+1] = tmp;
			if (crc16_ccitt_false(subframe->data, subframe->len+2)) {
				data.type = EMPTY;
				break;
			}

			/* Parse the subframe */
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

					if (self->metadata.data.burstkill_timer == 0xFFFF) {
						data.data.info.burstkill_status = -1;
					} else {
						data.data.info.burstkill_status = self->metadata.data.burstkill_timer;
					}
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
					data.type = POSITION;

					data.data.pos.x = rs41_subframe_x((RS41Subframe_GPSPos*)subframe);
					data.data.pos.y = rs41_subframe_y((RS41Subframe_GPSPos*)subframe);
					data.data.pos.z = rs41_subframe_z((RS41Subframe_GPSPos*)subframe);
					data.data.pos.dx = rs41_subframe_dx((RS41Subframe_GPSPos*)subframe);
					data.data.pos.dy = rs41_subframe_dy((RS41Subframe_GPSPos*)subframe);
					data.data.pos.dz = rs41_subframe_dz((RS41Subframe_GPSPos*)subframe);

					ecef_to_lla(&lat, &lon, &alt, data.data.pos.x, data.data.pos.y, data.data.pos.z);
					//printf("%.0f\n", alt);
					break;
				default:
					/* Unknown */
					data.type = UNKNOWN;
					break;
			}


			break;
		default:
			data.type = UNKNOWN;
			break;
	}

	return data;
}

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
	/*
	printf("{%d} ", s->frag_seq);
	for (i=0; i<69; i++) {
		printf("%f ", ((float*)m->data.rt_ref)[i]);
	}
	printf("\n");
	*/


}
