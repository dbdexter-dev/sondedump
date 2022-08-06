#ifndef threads_h
#define threads_h

#ifdef _MSC_VER
#include <windows.h>
typedef HANDLE thread_t;
typedef DWORD WINAPI thread_ret_t;
#else
#include <pthread.h>
typedef pthread_t thread_t;
typedef void* thread_ret_t;
#endif

thread_t thread_create(thread_ret_t (*function)(void*), void *args);
thread_ret_t thread_join(thread_t tid);

#endif
