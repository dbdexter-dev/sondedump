#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "wavfile.h"
#include "utils.h"

#define FILE_BUFFER_SIZE 32768
static union {
	uint8_t bytes[FILE_BUFFER_SIZE];
	int16_t words[FILE_BUFFER_SIZE/2];
	float floats[FILE_BUFFER_SIZE/4];
} _buffer;
static size_t _offset;

struct wave_header {
	char _riff[4];          /* Literally RIFF */
	uint32_t chunk_size;
	char _filetype[4];      /* Literally WAVE */

	char _fmt[4];           /* Literally fmt  */
	uint32_t subchunk_size;
	uint16_t audio_format;
	uint16_t num_channels;
	uint32_t sample_rate;
	uint32_t byte_rate;
	uint16_t block_align;
	uint16_t bits_per_sample;
	char _data[4];          /* Literally data */
	uint32_t subchunk2_size;
};

int
wav_parse(FILE *fd, int *samplerate, int *bps)
{
	struct wave_header header;

	if (!(fread(&header, sizeof(struct wave_header), 1, fd))) return 1;

	if (strncmp(header._riff, "RIFF", 4)) return 1;
	if (strncmp(header._filetype, "WAVE", 4)) return 1;
	if (header.num_channels != 1) return 1;

	if (!(*bps = header.bits_per_sample)) return 1;
	*samplerate = (int)header.sample_rate;

	_offset = (size_t)-1;

	return 0;
}

int
wav_read(float *dst, int bps, size_t count, FILE *fd)
{
	size_t copy_count;
	size_t i;

	while (count > 0) {
		/* If offset is at end of buffer, read more bytes */
		if (!_offset) {
			if (!fread(&_buffer.bytes, sizeof(_buffer.bytes), 1, fd)) return 0;
			_offset = 0;
		}

		/* Compute number of samples to send over */
		copy_count = MIN(count, sizeof(_buffer) / (bps/8) - _offset);

		/* Convert samples */
		switch (bps) {
			case 8:
				for (i=0; i<copy_count; i++) {
					*dst++ = _buffer.bytes[_offset++] - 128;
				}
				break;
			case 16:
				for (i=0; i<copy_count; i++) {
					*dst++ = _buffer.words[_offset++];
				}
				break;
			case 32:
				memcpy(dst, _buffer.floats + _offset, bps/8 * copy_count);
				dst += copy_count;
				_offset += copy_count;
				break;
			default:
				return 0;
		}

		count -= copy_count;
		_offset %= (sizeof(_buffer) / (bps/8));
	}

	return 1;
}
