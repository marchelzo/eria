#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

#include <stdbool.h>

#include "network.h"
#include "message.h"
#include "tsmap.h"

struct eria;
typedef struct eria Eria;

typedef struct input {
        int cursor;
        vec(char) data;
        struct input *prev;
        struct input *next;
} Input;

typedef struct buffer {
        enum { B_CHANNEL, B_SERVER, B_USER } type;
        enum { A_NONE, A_NORMAL, A_IMPORTANT } activity;
        char *name;
        Network *network;
        vec(Message *) messages;

        Input *input;
        Input *last;

        TSMap *tsm;
        bool complete_again;
} Buffer;

Input *
input_new(Input *prev, Input *next);

Buffer *
buffer_new(char const *name, Network *network, int type);

void
buffer_add(Buffer *b, Eria *state, Message *m);

#endif
