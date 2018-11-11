#define _POSIX_SOURCE

#include <stdio.h>
#include <ctype.h>
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

static void
write_sanitized(char const *s, FILE *f)
{
        for (char const *c = s; *c != '\0'; ++c) {
                switch (*c) {
                case 1:
                case 2:
                case 28:
                case 29:
                case 30:
                case 31:
                        break;
                case 3:
                        if (c[1] == '#')
                                c += 15;
                        else {
                                if (isdigit(c[1])) ++c;
                                if (isdigit(c[1])) ++c;
                                if (c[1] == ',')   ++c;
                                if (isdigit(c[1])) ++c;
                                if (isdigit(c[1])) ++c;
                        }
                        break;
                default:
                        fputc(*c, f);
                }
        }
}

void
msg_log(Message const *m, FILE *f)
{
        char time[64];
        strftime(time, sizeof time, "%F %H:%M:%S", &m->time);

        fputs(time, f);
        fputc('\t', f);
        write_sanitized(m->title, f);
        fputc('\t', f);
        write_sanitized(m->body, f);
        fputc('\n', f);

        fflush(f);
}
