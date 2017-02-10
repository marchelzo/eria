#ifndef ERIA_H_INCLUDED
#define ERIA_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>
#include <libsrsirc/irc.h>
#include <poll.h>
#include <termkey.h>
#include <time.h>
#include "vec.h"
#include "network.h"
#include "window.h"
#include "re.h"
#include "message.h"

typedef struct {
        char const *name;
        char const *server;
        unsigned short port;

        bool auto_connect;
        unsigned long long timeout; /* microseconds (0 means none) */

        char const *nick;
        char const *user;
        char const *real;
        
        struct {
                char const *mechanism;
                char const *key;
        } sasl;

        /* auto-join */
        char const *channels[ERIA_MAX_AUTOJOIN + 1];
} NetworkConfig;

typedef struct {
        NetworkConfig networks[ERIA_MAX_NETWORKS + 1];
        intmax_t room_list_timeout;
} Config;

typedef struct eria {
        Config *config;
        struct pollfd fds[1 + ERIA_MAX_NETWORKS];
        TermKey *tk;
        struct {
                Network *items[ERIA_MAX_NETWORKS];
                int count;
        } networks;
        Window *window;
        Window *root;
        intmax_t redraw_timeout;
        bool draw_rooms;
} Eria;

Message *
message_new(int type, char const *who, char const *what);

Message *
message_privmsg(char const *who, char const *what);

#endif
