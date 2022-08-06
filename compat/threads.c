#include "threads.h"


thread_t
thread_create(thread_ret_t (*function)(void*), void *args)
{
	thread_t retval;
#ifdef _MSC_VER
	retval = CreateThread(NULL, 0, function, args, 0, NULL);
#else
	pthread_create(&retval, NULL, function, args);
#endif

	return retval;
}

thread_ret_t
thread_join(thread_t tid)
{
	void* retval = NULL;
#ifdef _MSC_VER
	WaitForSingleObject(_tid, INFINITE);
#else
	pthread_join(tid, &retval);
#endif

	return retval;
}
