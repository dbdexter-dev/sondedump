/**
 * Platform-agnostic simple semaphore interface
 */
#ifndef semaphores_h
#define semaphores_h

#ifdef _MSC_VER
#include <windows.h>
typedef HANDLE semaphore_t;
#else
#include <semaphore.h>
typedef sem_t semaphore_t;
#endif

void semaphore_init(semaphore_t *sem, unsigned int value);
void semaphore_destroy(semaphore_t *sem);

void semaphore_post(semaphore_t *sem);
void semaphore_wait(semaphore_t *sem);

#endif
