#ifndef audio_h
#define audio_h

#include <stdlib.h>

#define BUFFER_SIZE 2048

/**
 * Initialize Portaudio backend
 *
 * @return 0 on success
 */
int audio_init(void);

/**
 * Open an audio device and start streaming audio from it
 */
int audio_open_device(int device_idx);

/**
 * Get number of devices
 */
int audio_get_num_devices(void);

/**
 * Get pointers to device names
 */
const char **audio_get_device_names(void);

/**
 * Read data from the previously initialized device
 *
 * @param ptr pointer to read data into
 * @param len number of samples to read
 * @return number of bytes read (0/1)
 */
int audio_read(float *ptr, size_t len);

/**
 * Deinitialize PortAudio
 */
int audio_deinit(void);

#endif
