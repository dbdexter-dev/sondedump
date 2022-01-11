#ifndef audio_h
#define audio_h

#define BUFFER_SIZE 2048

/**
 * Initialize a PortAudio audio input device
 *
 * @param device_num index of the device to intiialize, 0 to prompt the user
 * @return sample rate
 */
int audio_init(int device_num);

/**
 * Read data from the previously initialized device
 *
 * @param ptr pointer to read data into
 * @return number of bytes read (0/1)
 */
int audio_read(float *ptr);

/**
 * Deinitialize PortAudio
 */
int audio_deinit();

#endif
