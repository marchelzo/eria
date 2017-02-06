#include "buffer.h"
#include "util.h"

Buffer *
buffer_new(char const *name, Network *network, int type)
{
        Buffer *b = alloc(sizeof *b);

        b->name = sclone(name);
        b->network = network;
        b->cursor = 0;
        b->type = type;
        vec_init(b->messages);
        vec_init(b->input);

        return b;
}
