#ifndef audio_h
#define audio_h

#define BUFFER_SIZE 2048

int audio_init(int device_num);
int audio_read(float *ptr);
int audio_deinit();

#endif
