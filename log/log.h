#ifndef log_h
#define log_h

#include <stdlib.h>

#ifndef NDEBUG
#define log_debug(...) do_log_debug(__FILE__, __LINE__, __VA_ARGS__)
#define log_debug_hexdump(buf, len) do_log_debug_hexdump(__FILE__, __LINE__, buf, len)
#else
#define log_debug(...)
#endif

#define log_info(...) do_log_info(__FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) do_log_warn(__FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) do_log_error(__FILE__, __LINE__, __VA_ARGS__)

void log_enable(int enable);

void do_log_error(const char *tag, int line, const char *fmt, ...);
void do_log_warn(const char *tag, int line, const char *fmt, ...);
void do_log_info(const char *tag, int line, const char *fmt, ...);
void do_log_debug(const char *tag, int line, const char *fmt, ...);

void do_log_debug_hexdump(const char *tag, int line, const void *data, size_t len);

#endif
