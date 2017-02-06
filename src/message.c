#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#include "message.h"
#include "term.h"
#include "log.h"

static inline char *
color(char *dst, Color fg, Color bg)
{
        return dst + sprintf(
                dst,
                "\003#%02x%02x%02x,#%02x%02x%02x",
                fg.r, fg.g, fg.b,
                bg.r, bg.g, bg.b
        );
}


static size_t
fmt(char * restrict dst, char const * restrict fmt, va_list ap)
{
        bool bold = false;
        bool italic = false;
        bool underline = false;

        bool fg = false;
        bool bg = false;

        char const *begin = dst;

        Color fgc = C_DEFAULT;
        Color bgc = C_DEFAULT;

        char fb[4096];
        if (strlen(fmt) >= sizeof fb)
                assert(!"format string too large");
        strcpy(fb, fmt);

        char *f = fb;

        while (*fmt != '\0') {
                switch (*fmt++) {
                case '\\':
                        *f++ = *fmt++;
                        break;
                case '*':
                        *f++ = --bold ? 2 : 1;
                        break;
                case '/':
                        *f++ = --italic ? 29 : 28;
                        break;
                case '_':
                        *f++ = --underline ? 31 : 30;
                        break;
                case '^':
                        fgc = --fg ? va_arg(ap, Color) : C_DEFAULT;
                        f = color(f, fgc, bgc);
                        break;
                case '$':
                        bgc = --bg ? va_arg(ap, Color) : C_DEFAULT;
                        f = color(f, fgc, bgc);
                        break;
                default:
                        *f++ = fmt[-1];
                }
        }

        *f = '\0';

        return vsprintf(dst, fb, ap);
}

Message *
msg(char const *tfmt, char const *bfmt, ...)
{
        static char buffer[1 << 16];
        va_list ap;

        va_start(ap, bfmt);

        size_t tlen = fmt(buffer, tfmt, ap);
        size_t blen = fmt(buffer + tlen + 1, bfmt, ap);

        va_end(ap);

        Message *msg = alloc(sizeof *msg + tlen + blen + 2);

        memcpy(msg->data, buffer, tlen + blen + 2);

        time_t clock = time(NULL);
        localtime_r(&clock, &msg->time);

        msg->title = msg->data;
        msg->body = msg->data + tlen + 1;

        msg->important = false;

        return msg;
}
