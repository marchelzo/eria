#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include <stdarg.h>

#define L(...) log_write(__FILE__, __LINE__, __func__, __VA_ARGS__)

void
log_write(char const *file, int line, char const *func, char const *fmt , ...)
#ifdef __GNUC__
__attribute__((format(printf, 4, 5)))
#endif
;

#endif
