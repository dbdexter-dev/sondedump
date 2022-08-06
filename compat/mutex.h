#ifndef mutex_h
#define mutex_h


#ifdef _MSC_VER
#include <windows.h>
typedef HANDLE mutex_t;
#else
#include <pthread.h>
typedef pthread_mutex_t mutex_t;
#endif

void mutex_init(mutex_t *mutex);
void mutex_destroy(mutex_t *mutex);

void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif
