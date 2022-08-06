#include <stdlib.h>
#include "mutex.h"

void
mutex_init(mutex_t *mutex)
{
#ifdef _MSC_VER
	*mutex = CreateMutex(NULL, FALSE, NULL);
#else
	pthread_mutex_init(mutex, NULL);
#endif
}

void
mutex_destroy(mutex_t *mutex)
{
#ifdef _MSC_VER
	CloseHandle(*mutex);
#else
	pthread_mutex_destroy(mutex);
#endif
}

void
mutex_lock(mutex_t *mutex)
{
#ifdef _MSC_VER
	WaitForSingleObject(mutex, INFINITE);
#else
	pthread_mutex_lock(mutex);
#endif
}

void
mutex_unlock(mutex_t *mutex)
{
#ifdef _MSC_VER
	ReleaseMutex(*mutex);
#else
	pthread_mutex_unlock(mutex);
#endif
}
