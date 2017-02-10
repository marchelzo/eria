#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

#include <stdbool.h>

#include "network.h"
#include "message.h"
#include "tsmap.h"

struct eria;
typedef struct eria Eria;

typedef struct buffer {
        enum { B_CHANNEL, B_SERVER, B_USER } type;
        enum { A_NONE, A_NORMAL, A_IMPORTANT } activity;
        char *name;
        Network *network;
        vec(Message *) messages;
        vec(char) input;
        int cursor;
        TSMap *tsm;
        bool complete_again;
} Buffer;

Buffer *
buffer_new(char const *name, Network *network, int type);

void
buffer_add(Buffer *b, Eria *state, Message *m);

#endif
