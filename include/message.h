#ifndef MSG_H_INCLUDED
#define MSG_H_INCLUDED

#include <time.h>
#include <stdbool.h>

typedef struct {
        bool important;
        struct tm time;

        char const *title;
        char const *body;

        char data[];
} Message;

Message *
msg(char const *tfmt, char const *bfmt, ...);

#endif
