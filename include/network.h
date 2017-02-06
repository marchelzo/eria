#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <libsrsirc/irc.h>
#include "vec.h"
#include "re.h"

struct buffer;
typedef struct buffer Buffer;

typedef struct network {
        char const *name;
        irc *connection;
        re_pat *nick_regex;
        vec(Buffer *) buffers;
} Network;

#endif
