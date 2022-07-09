#include <stdarg.h>
#include <stdio.h>
#include "log.h"

/* Size of the path leading up to the project root, set by cmake at compile time */
#ifndef SOURCE_BASE_PATH_SIZE
#define SOURCE_BASE_PATH_SIZE 0
#endif

#define COLOR_DEFAULT "\033[;0m"
#define COLOR_ERR "\033[;31m"
#define COLOR_WARN "\033[;33m"
#define COLOR_INFO "\033[;32m"
#define COLOR_DEBUG "\033[;34m"

void
do_log_error(const char *tag, const char *fmt, ...)
{
	va_list args;

	printf("[" COLOR_ERR "%s" COLOR_DEFAULT "] ", tag + SOURCE_BASE_PATH_SIZE);

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
}

void
do_log_warn(const char *tag, const char *fmt, ...)
{
	va_list args;

	printf("[" COLOR_WARN "%s" COLOR_DEFAULT "] ", tag + SOURCE_BASE_PATH_SIZE);

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
}

void
do_log_info(const char *tag, const char *fmt, ...)
{
	va_list args;

	printf("[" COLOR_INFO "%s" COLOR_DEFAULT "] ", tag + SOURCE_BASE_PATH_SIZE);

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
}

void
do_log_debug(const char *tag, const char *fmt, ...)
{
	va_list args;

	printf("[" COLOR_DEBUG "%s" COLOR_DEFAULT "] ", tag + SOURCE_BASE_PATH_SIZE);

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
}

void
do_log_debug_hexdump(const char *tag, const uint8_t *data, int len)
{
	int i;

	printf("[" COLOR_DEBUG "%s" COLOR_DEFAULT "]", tag + SOURCE_BASE_PATH_SIZE);

	for (i=0; i < len; i++) {
		if (!(i % 16)) printf("\n%04x\t", i);
		if (!(i % 8)) printf(" ");
		printf("%02x ", data[i]);
	}
	printf("\n");
}
