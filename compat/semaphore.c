#include "semaphore.h"

void
semaphore_init(semaphore_t *sem, unsigned int value)
{
#ifdef _MSC_VER
	*sem = CreateSemaphore(NULL, value, MAX_SEM_COUNT, NULL);
#else
	sem_init(sem, 0, value);
#endif
}

void
semaphore_destroy(semaphore_t *sem)
{
#ifdef _MSC_VER
	CloseHandle(*sem);
#else
	sem_destroy(sem);
#endif
}

void
semaphore_post(semaphore_t *sem)
{
#ifdef _MSC_VER
	ReleaseSemaphore(*sem, 1, NULL);
#else
	sem_post(sem);
#endif
}

void
semaphore_wait(semaphore_t *sem)
{
#ifdef _MSC_VER
	WaitForSingleObject(*sem, INFINITE);
#else
	sem_wait(sem);
#endif
}

