#include <inttypes.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>

#include "alloc.h"
#include "vec.h"
#include "term.h"
#include "utf8.h"
#include "log.h"

#define CELL(t, y, x) ((t)->buffers[(t)->i][(t)->cols * (y) + (x)])
#define ALT_CELL(t, y, x) ((t)->buffers[!(t)->i][(t)->cols * (y) + (x)])

static Cell const EMPTY;

inline static void
flush(Term *t)
{
        char const *output = t->buffer.items;

        while (t->buffer.count != 0) {
                int n = write(STDOUT_FILENO, output, t->buffer.count);
                if (n == -1)
                        continue;
                output += n;
                t->buffer.count -= n;
        }
}

inline static bool
color_equal(Color a, Color b)
{
        return a.r == b.r
            && a.g == b.g
            && a.b == b.b;
}

inline static bool
video_equal(Video a, Video b)
{
        return color_equal(a.fg, b.fg)
            && color_equal(a.bg, b.bg)
            && a.reverse == b.reverse
            && a.bold == b.bold
            && a.underline == b.underline
            && a.italic == b.italic;
}

inline static bool
cell_empty(Cell const *c)
{
        return c->data[0] == '\0';
}

inline static bool
cell_equal(Cell const *a, Cell const *b)
{
        if (cell_empty(a) && cell_empty(b))
                return true;

        return video_equal(a->video, b->video)
            && strcmp(a->data, b->data) == 0;
}

inline static void
render(Term *t, char const *s)
{
        vec_push_n(t->buffer, s, strlen(s));
}

inline static void
render_video(Term *t)
{
        char buffer[128];

        Video v = t->video;
        Color fg = v.fg;
        Color bg = v.bg;

        if (color_equal(fg, C_DEFAULT))
                fg = (Color) { 220, 220, 220 };

        if (color_equal(bg, C_DEFAULT))
                bg = (Color) { 20, 20, 20 };

        int n = sprintf(buffer, "\033[0;");

        if (v.bold)      n += sprintf(buffer + n, "1;");
        if (v.italic)    n += sprintf(buffer + n, "3;");
        if (v.underline) n += sprintf(buffer + n, "4;");
        if (v.reverse)   n += sprintf(buffer + n, "7;");

        n += sprintf(
                buffer + n,
                "38;2;%"PRIu8";%"PRIu8";%"PRIu8";",
                fg.r, fg.g, fg.b
        );

        n += sprintf(
                buffer + n,
                "48;2;%"PRIu8";%"PRIu8";%"PRIu8";",
                bg.r, bg.g, bg.b
        );

        buffer[n - 1] = 'm';

        vec_push_n(t->buffer, buffer, n);
}

inline static void
render_move(Term *t, int y, int x)
{
        char buffer[64];
        sprintf(buffer, "\033[%d;%dH", y + 1, x + 1);
        render(t, buffer);
}

inline static int
skip_empty(Term * const t, int y, int x)
{
        int n = 0;

        while (x + n < t->cols && cell_empty(&CELL(t, y, x + n)))
                ++n;

        return n;
}

static void
render_line(Term *t, int y)
{
        int x = 0;

        for (;;) {
                int skip = skip_empty(t, y, x);
                if (x + skip == t->cols)
                        break;
                if (x == 0 || skip > 0)
                        render_move(t, y, x + skip);
                x += skip;
                t->video = CELL(t, y, x).video;
                render_video(t);
                while (x < t->cols && video_equal(CELL(t, y, x).video, t->video))
                        render(t, CELL(t, y, x++).data);
        }
}

void
term_resize(Term *t, int rows, int cols)
{
        size_t size = sizeof (Cell) * rows * cols;

        resize(t->buffers[0], size);
        resize(t->buffers[1], size);

        memset(t->buffers[0], 0, size);
        memset(t->buffers[1], 0, size);

        t->rows = rows;
        t->cols = cols;
}

void
term_init(Term *t, int rows, int cols)
{
        t->y = 0;
        t->x = 0;
        t->rows = 0;
        t->cols = 0;
        t->i = 0;
        t->buffers[0] = NULL;
        t->buffers[1] = NULL;
        t->video = V_NORMAL;
        vec_init(t->buffer);
        term_resize(t, rows, cols);
}

void
term_move(Term *t, int y, int x)
{
        t->y = y;
        t->x = x;
}

void
term_write(Term *t, Video v, char const *s)
{
        int width;
        int i = 0;

        while (s[i] != '\0') {
                int bytes = utf8_next(s + i, &width);
                memcpy(CELL(t, t->y, t->x).data, s + i, bytes);
                CELL(t, t->y, t->x).data[bytes] = '\0';
                CELL(t, t->y, t->x).video = v;
                t->x += width;
                i += bytes;
        }
}

void
term_printf(Term *t, Video v, char const *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        
        char buffer[1024];
        vsnprintf(buffer, sizeof buffer, fmt, ap);

        term_write(t, v, buffer);

        va_end(ap);
}

void
term_mvprintf(Term *t, int y, int x, Video v, char const *fmt, ...)
{
        t->y = y;
        t->x = x;

        va_list ap;
        va_start(ap, fmt);
        
        char buffer[1024];
        vsnprintf(buffer, sizeof buffer, fmt, ap);

        term_write(t, v, buffer);

        va_end(ap);
}

void
term_clear(Term *t)
{
        for (int y = 0; y < t->rows; ++y)
                for (int x = 0; x < t->cols; ++x)
                        CELL(t, y, x) = EMPTY;
}

void
term_flush(Term *t)
{
        render(t, "\033[2J");

        for (int y = 0; y < t->rows; ++y)
                render_line(t, y);
        
        render_move(t, t->y, t->x);        
        
        flush(t);

        t->i += 1;
}
