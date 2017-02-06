#ifndef UTF8_H_INCLUDED
#define UTF8_H_INCLUDED

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "unicode.h"
#include "log.h"

inline static int
next_utf8(const char *str, int len, uint32_t *cp)
{
        unsigned char b0 = (str++)[0];
        int nbytes;

        if (!len)
                return -1;

        if (!b0)
                return -1;
        else if (b0 < 0x80) { // ASCII
                *cp = b0; return 1;
        }
        else if (b0 < 0xc0) // C1 or continuation
                return -1;
        else if (b0 < 0xe0) {
                nbytes = 2; *cp = b0 & 0x1f;
        }
        else if (b0 < 0xf0) {
                nbytes = 3; *cp = b0 & 0x0f;
        }
        else if (b0 < 0xf8) {
                nbytes = 4; *cp = b0 & 0x07;
        }
        else
                return -1;

        if (len < nbytes)
                return -1;

        for (int i = 1; i < nbytes; i++) {
                b0 = (str++)[0];
                if (!b0)
                        return -1;

                *cp <<= 6;
                *cp |= b0 & 0x3f;
        }

        return nbytes;
}

inline static bool
utf8_valid(char const *str, int len)
{
        uint32_t cp;

        while (len != 0) {
                int n = next_utf8(str, len, &cp);
                if (n == -1)
                        return false;
                len -= n;
                str += n;
        }

        return true;
}

inline static int
utf8_fit(char const *str, int len, int cols)
{
        int n = 0;
#define SKIP { bytes = 1; goto next; }
        while (len != 0) {
                int bytes;
                int width = 0;

                /* handle color codes -- bad! */
                if (str[0] == 3) {
                        if (str[1] == '#') {
                                if (len > 16)
                                        bytes = 16;
                                else
                                        bytes = len;
                        } else {
                                bytes = 1;
                                if (isdigit(str[bytes])) ++bytes;
                                if (isdigit(str[bytes])) ++bytes;
                                if (str[bytes] == ',')   ++bytes;
                                if (bytes > 3 && isdigit(str[bytes])) ++bytes;
                                if (bytes > 3 && isdigit(str[bytes])) ++bytes;
                        }
                        goto next;
                }

                uint32_t cp;
                bytes = next_utf8(str, len, &cp);
                if (bytes == -1)
                        SKIP;

                /* Skip C0 or C1 controls */
                if (cp < 0x20 || (cp >= 0x80 && cp < 0xa0))
                        SKIP;

                width = mk_wcwidth(cp);
                if (width == -1)
                        SKIP;

                if (cols - width < 0)
                        break;

                cols -= width;
next:
                str += bytes;
                len -= bytes;
                n += bytes;
        }
#undef SKIP
        return n;
}

inline static int
utf8_width(char const *str, int len)
{
        int w = 0;
#define SKIP { bytes = 1; goto next; }
        while (len != 0) {
                uint32_t cp;
                int bytes = next_utf8(str, len, &cp);
                if (bytes == -1)
                        SKIP;

                /* Skip C0 or C1 controls */
                if (cp < 0x20 || (cp >= 0x80 && cp < 0xa0))
                        SKIP;

                int width = mk_wcwidth(cp);
                if (width == -1)
                        SKIP;

                w += width;
next:
                str += bytes;
                len -= bytes;
        }
#undef SKIP
        return w;
}

inline static int
utf8_next(char const *str, int *w)
{
        *w = 0;
        int n = 0;

        while (*str != '\0') {
                uint32_t cp;
                int bytes = next_utf8(str, 16, &cp);
                if (bytes == -1)
                        break;

                /* Skip C0 or C1 controls */
                if (cp < 0x20 || (cp >= 0x80 && cp < 0xa0))
                        break;

                int width = mk_wcwidth(cp);
                if (width == -1)
                        break;

                if (*w > 0 && width > 0)
                        break;

                *w += width;
                n += bytes;
                str += bytes;
        }

        return n;
}

#endif
