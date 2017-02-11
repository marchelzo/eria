#include "eria.h"
#include "buffer.h"
#include "util.h"
#include "tsmap.h"

Buffer *
buffer_new(char const *name, Network *network, int type)
{
        Buffer *b = alloc(sizeof *b);

        b->name = sclone(name);
        b->network = network;
        b->cursor = 0;
        b->activity = A_NONE;
        b->type = type;
        b->tsm = tsmap_new();
        vec_init(b->messages);
        vec_init(b->input);

        b->hi = 0;
        vec_init(b->history);

        return b;
}

static void
scroll(Window *w, Buffer const *b)
{
        switch (w->type) {
        case W_VS:
        case W_HS:
                scroll(w->one, b);
                scroll(w->two, b);
                break;
        default:
                if (w->buffer == b && w->scroll != 0)
                        ++w->scroll;
        }
}

void
buffer_add(Buffer *b, Eria *state, Message *m)
{
        vec_push(b->messages, m);
        scroll(state->root, b);
}
