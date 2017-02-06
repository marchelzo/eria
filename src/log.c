#include <stdio.h>
#include <stdarg.h>

void
log_write(char const *file, int line, char const *func, char const *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        fprintf(stderr, "%s:%d %s(): ", file, line, func);
        vfprintf(stderr, fmt, ap);
        fputc('\n', stderr);
        va_end(ap);
}
