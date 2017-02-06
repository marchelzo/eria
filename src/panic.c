#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdnoreturn.h>

noreturn void
panic(char const *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        fputc('\n', stderr);
        va_end(ap);
        exit(EXIT_FAILURE);
}

noreturn void
epanic(char const *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, ": %s", strerror(errno));
        fputc('\n', stderr);
        va_end(ap);
        exit(EXIT_FAILURE);
}
