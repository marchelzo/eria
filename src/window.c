#include <assert.h>

#include "alloc.h"
#include "window.h"

#define SIBLING(w) ((w)->parent->one == (w) ? (w)->parent->two : (w)->parent->one)

inline static int
min(int a, int b)
{
        return (a < b) ? a : b;
}

static Window *
new(Window *parent, int height, int width, int y, int x)
{
        Window *w = alloc(sizeof *w);

        w->type = W_W;
        w->parent = parent;
        w->height = height;
        w->width = width;
        w->y = y;
        w->x = x;
        w->buffer = NULL;
        w->scroll = 0;
        w->resize = false;

        return w;
}

static Window *
find(Window *w, int y, int x)
{
        switch (w->type) {
        case W_VS:
                if (x < w->left->x + w->left->width)
                        return find(w->left, y, x);
                else
                        return find(w->right, y, x);
        case W_HS:
                if (y < w->top->y + w->top->height)
                        return find(w->top, y, x);
                else
                        return find(w->bot, y, x);
        case W_W:
                return w;
        default:
                assert(false);
        }
}

static Window *
root(Window *w)
{
        if (w->parent == NULL)
                return w;
        return root(w->parent);
}

static void
hbalance(Window *w)
{
        switch (w->type) {
        case W_HS:
                w->one->width = w->width;
                w->two->width = w->width;
                w->one->x = w->x;
                w->two->x = w->x;
                break;
        case W_VS:
                w->one->width = (w->width / 2) + (w->width & 1);
                w->two->width = w->width - w->one->width;
                w->one->x = w->x;
                w->two->x = w->x + w->one->width;
                break;
        case W_W:
                return;
        }

        hbalance(w->one);
        hbalance(w->two);
}

static void
vbalance(Window *w)
{
        switch (w->type) {
        case W_HS:
                w->one->height = (w->height / 2) + (w->height & 1);
                w->two->height = w->height - w->one->height;
                w->one->y = w->y;
                w->two->y = w->y + w->one->height;
                break;
        case W_VS:
                w->one->height = w->height;
                w->two->height = w->height;
                w->one->y = w->y;
                w->two->y = w->y;
                break;
        case W_W:
                return;
        }

        vbalance(w->top);
        vbalance(w->bot);
}

Window *
window_root(int height, int width)
{
        return new(NULL, height, width, 0, 0);
}

Window *
window_next(Window *w)
{
        assert(w->type == W_W);

        Window *p = w->parent;

        if (p == NULL)
                return w;

        while (p != NULL && (p->two == w || p->two == NULL)) {
                w = p;
                p = p->parent;
        }

        w = (p == NULL) ? w : p->two;

        while (w->type != W_W)
                w = w->one;

        return w;
}

Window *
window_prev(Window *w)
{
        assert(w->type == W_W);

        Window *p = w->parent;

        if (p == NULL)
                return w;

        while (p != NULL && (p->one == w || p->one == NULL)) {
                w = p;
                p = p->parent;
        }

        w = (p == NULL) ? w : p->one;

        while (w->type != W_W)
                w = w->two;

        return w;
}

Window *
window_right(Window *w)
{
        Window *r = root(w);

        if (w->x + w->width == r->width)
                return w;
        else
                return find(r, w->y, w->x + w->width);
}

Window *
window_left(Window *w)
{
        Window *r = root(w);

        if (w->x == 0)
                return w;
        else
                return find(r, w->y, w->x - 1);
}

Window *
window_up(Window *w)
{
        Window *r = root(w);

        if (w->y == 0)
                return w;
        else
                return find(r, w->y - 1, w->x);
}

Window *
window_down(Window *w)
{
        Window *r = root(w);

        if (w->y + w->height == r->height)
                return w;
        else
                return find(r, w->y + w->height, w->x);
}

void
window_grow_y(Window *w, int dy)
{
        Window *p = w->parent;

        while (p != NULL && p->type != W_HS) {
                w = p;
                p = p->parent;
        }

        if (p == NULL)
                return;

        w->height += dy;
        SIBLING(w)->height -= dy;

        if (w == p->top)
                p->bot->y += dy;
        else
                w->y -= dy;

        vbalance(p->top);
        vbalance(p->bot);
}

void
window_grow_x(Window *w, int dx)
{
        Window *p = w->parent;

        while (p != NULL && p->type != W_VS) {
                w = p;
                p = p->parent;
        }

        if (p == NULL)
                return;

        w->width += dx;
        SIBLING(w)->width -= dx;

        if (w == p->left)
                p->right->x += dx;
        else
                w->x -= dx;

        hbalance(p->left);
        hbalance(p->right);
}

void
window_set_height(Window *w, int height)
{
        window_grow_y(w, height - w->height);
}

void
window_set_width(Window *w, int width)
{
        window_grow_x(w, width - w->width);
}

void
window_hsplit(Window *w, Buffer *b, int size)
{
        assert(w->type == W_W);

        int bh = (size == -1) ? (w->height / 2) : min(size, w->height);
        int th = w->height - bh;

        Buffer *old = w->buffer;
        int scroll = w->scroll;
        bool resize = w->resize;

        w->type = W_HS;
        w->top = new(w, th, w->width, w->y, w->x);
        w->bot = new(w, bh, w->width, w->y + th, w->x);

        w->top->scroll = scroll;
        w->top->buffer = old;
        w->top->resize = resize;
        w->bot->buffer = b;
}

void
window_vsplit(Window *w, Buffer *b, int size)
{
        assert(w->type == W_W);

        int rw = (size == -1) ? (w->width / 2) : min(size, w->width);
        int lw = w->width - rw;

        Buffer *old = w->buffer;

        w->type = W_VS;
        w->left = new(w, w->height, lw, w->y, w->x);
        w->right = new(w, w->height, rw, w->y, w->x + lw);

        w->left->buffer = old;
        w->right->buffer = b;
}

Window *
window_delete(Window *w)
{
        if (w->parent == NULL)
                return w;

        Window *parent = w->parent;
        Window *sibling = SIBLING(w);

        void (*fix)(Window *);
        switch (parent->type) {
        case W_HS: fix = vbalance; break;
        case W_VS: fix = hbalance; break;
        default:   assert(false);
        }

        parent->type = sibling->type;

        if (sibling->type == W_W) {
                parent->buffer = sibling->buffer;
                parent->scroll = sibling->scroll;
                parent->resize = sibling->resize;
        } else {
                parent->one = sibling->one;
                parent->two = sibling->two;
                parent->one->parent = parent;
                parent->two->parent = parent;
                fix(parent);
        }

        free(sibling);
        free(w);

        return find(parent, parent->x, parent->y);
}
