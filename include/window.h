#ifndef WINDOW_H_INCLUDED
#define WINDOW_H_INCLUDED

#include <stdbool.h>
#include "buffer.h"

typedef struct window Window;

struct window {
        enum { W_W, W_HS, W_VS } type;
        Window *parent;
        int height;
        int width;
        int y;
        int x;
        union {
                struct {
                        Window *one;
                        Window *two;
                };
                struct {
                        Window *left;
                        Window *right;
                };
                struct {
                        Window *top;
                        Window *bot;
                };
                struct {
                        Buffer *buffer;
                        int scroll;
                        bool resize;
                        bool search;
                        bool nicks;
                };
        };
};

Window *
window_delete(Window *w);

void
window_vsplit(Window *w, Buffer *new, int size);

void
window_hsplit(Window *w, Buffer *new, int size);

void
window_set_height(Window *w, int height);

void
window_set_width(Window *w, int width);

void
window_resize(Window *w, int height, int width);

void
window_grow_x(Window *w, int dx);

void
window_grow_y(Window *w, int dy);

Window *
window_root(int height, int width);

Window *
window_next(Window *w);

Window *
window_prev(Window *w);

Window *
window_right(Window *w);

Window *
window_left(Window *w);

Window *
window_up(Window *w);

Window *
window_down(Window *w);

#endif
