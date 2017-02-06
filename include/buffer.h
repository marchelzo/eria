#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED

#include "network.h"
#include "message.h"

typedef struct buffer {
        enum { B_CHANNEL, B_SERVER, B_USER } type;
        enum { A_IMPORTANT, A_NORMAL, A_NONE } activity;
        char *name;
        Network *network;
        vec(Message *) messages;
        vec(char) input;
        int cursor;
} Buffer;

Buffer *
buffer_new(char const *name, Network *network, int type);

#endif
