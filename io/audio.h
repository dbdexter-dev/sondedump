#ifndef audio_h
#define audio_h

#define RINGBUFFER_SIZE (1024*65536)

int audio_init(int device_num);
int audio_read(float *ptr);
int audio_deinit();

#endif
