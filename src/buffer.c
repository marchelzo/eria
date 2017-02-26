#include "eria.h"
#include "buffer.h"
#include "util.h"
#include "tsmap.h"
#include "message.h"

Input *
input_new(Input *prev, Input *next)
{
        Input *input = alloc(sizeof *input);
        input->prev = prev;
        input->next = next;
        input->cursor = 0;
        vec_init(input->data);
        return input;
}

Buffer *
buffer_new(char const *name, Network *network, int type)
{
        Buffer *b = alloc(sizeof *b);

        b->name = sclone(name);
        b->network = network;
        b->activity = A_NONE;
        b->type = type;
        b->tsm = tsmap_new();
        vec_init(b->messages);
        b->input = b->last = input_new(NULL, NULL);

        char path[4096];
        char const *home = getenv("HOME");
        snprintf(path, sizeof path, "%s/.eria/logs/%s.%s", home, network->name, name);

        b->log = fopen(path, "a");

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

        if (b->log != NULL)
                msg_log(m, b->log);
}
