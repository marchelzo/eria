#ifndef TERM_H_INCLUDED
#define TERM_H_INCLUDED

#include <inttypes.h>
#include <stdarg.h>
#include "vec.h"

typedef struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
} Color;

typedef struct {
        Color fg;
        Color bg;
        unsigned reverse   : 1;
        unsigned bold      : 1;
        unsigned underline : 1;
        unsigned italic    : 1;
} Video;

typedef struct {
        Video video;
        char data[16];
} Cell;

typedef struct {
        int rows;
        int cols;
        int y;
        int x;
        unsigned i : 1;
        Cell *buffers[2];
        Video video;
        vec(char) buffer;
} Term;

#define C_DEFAULT ((Color){ 1, 1, 1 })
#define V_NORMAL  ((Video){ .fg = C_DEFAULT, .bg = C_DEFAULT })

void
term_resize(Term *t, int rows, int cols);

void
term_init(Term *t, int rows, int cols);

void
term_move(Term *t, int y, int x);

void
term_write(Term *t, Video v, char const *s);

void
term_printf(Term *t, Video v, char const *fmt, ...);

void
term_mvprintf(Term *t, int y, int x, Video v, char const *fmt, ...);

void
term_clear(Term *t);

void
term_flush(Term *t);

#endif
