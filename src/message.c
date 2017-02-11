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
fmt(char * restrict dst, char const * restrict fmt, va_list *ap)
{
        bool bold = false;
        bool italic = false;
        bool underline = false;

        bool fg = false;
        bool bg = false;

        char const *begin = dst;

        Color fgc = C_DEFAULT;
        Color bgc = C_DEFAULT;

        while (*fmt != '\0') {
                switch (*fmt++) {
                case '\\':
                        *dst++ = *fmt++;
                        break;
                case '*':
                        *dst++ = --bold ? 2 : 1;
                        break;
                case '/':
                        *dst++ = --italic ? 29 : 28;
                        break;
                case '_':
                        *dst++ = --underline ? 31 : 30;
                        break;
                case '^':
                        fgc = --fg ? va_arg(*ap, Color) : C_DEFAULT;
                        dst = color(dst, fgc, bgc);
                        break;
                case '$':
                        bgc = --bg ? va_arg(*ap, Color) : C_DEFAULT;
                        dst = color(dst, fgc, bgc);
                        break;
                case '%':;
                        char const *s = va_arg(*ap, char const *);
                        if (s == NULL) s = "";
                        size_t n = strlen(s);
                        memcpy(dst, s, n);
                        dst += n;
                        break;
                default:
                        *dst++ = fmt[-1];
                }
        }

        *dst++ = '\0';

        return dst - begin;
}

Message *
msg(char const *tfmt, char const *bfmt, ...)
{
        static char buffer[1 << 16];
        va_list ap;

        va_start(ap, bfmt);

        size_t tlen = fmt(buffer, tfmt, &ap);
        size_t blen = fmt(buffer + tlen, bfmt, &ap);

        va_end(ap);

        Message *msg = alloc(sizeof *msg + tlen + blen);

        memcpy(msg->data, buffer, tlen + blen);

        time_t clock = time(NULL);
        localtime_r(&clock, &msg->time);

        msg->title = msg->data;
        msg->body = msg->data + tlen;

        msg->important = false;

        return msg;
}
