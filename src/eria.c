#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <termkey.h>
#include <time.h>
#include <sys/time.h>
#include <libsrsirc/irc.h>
#include <libsrsirc/irc_ext.h>
#include <libsrsirc/irc_track.h>
#include <libsrsirc/util.h>

#include "eria.h"
#include "buffer.h"
#include "config.h"
#include "panic.h"
#include "ui.h"
#include "input.h"
#include "window.h"
#include "message.h"
#include "util.h"
#include "log.h"

static Color const quit_color = { 240, 60, 60 };
static Color const join_color = { 60, 240, 60 };
static Color const misc_color = { 233, 45, 250 };

static void *
try_connect(void *ctx)
{
        return irc_connect(ctx) ? ctx : NULL;
}

static void
set_nick_pattern(Network *network, char const *nick)
{
        if (network->nick_regex != NULL)
                re_free(network->nick_regex);

        static vec(char) pattern;
        pattern.count = 0;

        char start[] = "(^|[^a-zA-Z0-9_])";
        char end[] = "($|[^a-zA-Z0-9_])";

        vec_push_n(pattern, start, sizeof start - 1);
        while (*nick) switch (*nick++) {
        case '\\':
        case '[':
                vec_push(pattern, '\\');
        default:
                vec_push(pattern, lsi_ut_tolower(nick[-1], 0));
        }
        vec_push_n(pattern, end, sizeof end);

        network->nick_regex = re_compile(pattern.items);
}

static void
bell(void)
{
        write(STDOUT_FILENO, "\a", 1);
}

static bool
activity(Eria const *state)
{
        for (int i = 0; i < state->networks.count; ++i) {
                Network *network = state->networks.items[i];
                for (int i = 0; i < network->buffers.count; ++i)
                        if (network->buffers.items[i]->activity)
                                return true;
        }

        return false;
}

enum { ALL_CHANS_QUIT, ALL_CHANS_NICK };
static void
user_do_all_chans(irc *ctx, char const *ident, int type, ...)
{
        static vec(chanrep) chans;
        int n = irc_num_chans(ctx);
        vec_reserve(chans, n);

        va_list ap;
        va_start(ap, type);

        irc_all_chans(ctx, chans.items, n);

        userrep user;

        char const *reason;
        char const *old;
        Message *m;

        char nick[128];
        lsi_ut_ident2nick(nick, sizeof nick, ident);

        switch (type) {
        case ALL_CHANS_QUIT:
                reason = va_arg(ap, char const *);
                m = msg(
                        "^<--^",
                        "^%s^ has quit (%s)",
                        quit_color,
                        ui_nick_color(nick),
                        nick,
                        reason
                );
                break;
        case ALL_CHANS_NICK:
                old = va_arg(ap, char const *);
                m = msg(
                        "^--^",
                        "^%s^ is now known as ^%s^",
                        misc_color,
                        ui_nick_color(old),
                        ui_nick_color(nick),
                        old,
                        nick
                );
                break;
        }

        for (int i = 0; i < n; ++i) {
                if (irc_member(ctx, &user, chans.items[i].name, ident) != NULL) {
                        Buffer *buffer = chans.items[i].tag;
                        if (buffer != NULL) switch (type) {
                        case ALL_CHANS_QUIT:
                                vec_push(buffer->messages, m);
                                break;
                        case ALL_CHANS_NICK:
                                vec_push(buffer->messages, m);
                                break;
                        }
                }
        }

        va_end(ap);
}

static void
react(Eria *state, Network *network, tokarr tokens)
{
        irc *ctx = network->connection;
        char const *me = irc_mynick(ctx);
        char nick[64];

        char lower[512];
        char raw[512] = {0};

        if (tokens[0] != NULL) {
                lsi_ut_ident2nick(nick, sizeof nick, tokens[0]);
                strcat(raw, tokens[0]);
                strcat(raw, " ");
        }

        for (int i = 1; tokens[i] != NULL; ++i) {
                strcat(raw, " " + (tokens[i - 1] == NULL));
                strcat(raw, tokens[i]);
        }

        Message *raw_msg = msg("[server]", "%s", raw);
        vec_push(network->buffers.items[0]->messages, raw_msg);

#define CASE(s) if (strcmp(tokens[1], #s) == 0) {
#define END     return; }
        CASE(PRIVMSG)
                Buffer *b = NULL;

                lsi_ut_strtolower(lower, sizeof lower, tokens[3], 0);

                bool mentions_me = (network->nick_regex != NULL)
                                && re_match(network->nick_regex, lower, NULL);

                if (strcmp(me, tokens[2]) == 0) {
                        userrep u;
                        irc_user(ctx, &u, nick);
                        b = u.tag;
                        if (b == NULL) {
                                b = buffer_new(sclone(nick), network, B_USER);
                                vec_push(network->buffers, b);
                                bell();
                                irc_tag_user(ctx, nick, b, false);
                        }
                } else {
                        chanrep chan;
                        irc_chan(ctx, &chan, tokens[2]);
                        b = chan.tag;
                }

                Message *m;

                char action[] = "\001ACTION";
                if (strncmp(tokens[3], action, sizeof action - 1) == 0) {
                        m = msg("^\\*^", "^%s^ %s", misc_color, ui_nick_color(nick), nick, tokens[3] + sizeof action - 1);
                } else {
                        m = msg("^%s^", "%s", ui_nick_color(nick), nick, tokens[3]);
                }

                m->important = mentions_me;

                if ((b->type == B_USER || m->important) && b != state->window->buffer) {
                        b->activity = true;
                        bell();
                }

                vec_push(b->messages, m);
        END

        CASE(JOIN)
                if (strcmp(me, nick) == 0) {
                        Buffer *b = buffer_new(tokens[2], network, B_CHANNEL);
                        irc_tag_chan(ctx, tokens[2], b, false);
                        vec_push(network->buffers, b);
                        state->window->buffer = b;
                } else {
                        chanrep chan;
                        irc_chan(ctx, &chan, tokens[2]);
                        Buffer *b = chan.tag;
                        if (b != NULL) {
                                vec_push(
                                        b->messages,
                                        msg(
                                                "^-->^",
                                                "^%s^ (%s) has joined %s",
                                                join_color,
                                                ui_nick_color(nick),
                                                nick,
                                                tokens[0],
                                                b->name
                                        )
                                );
                        }
                }
        END

        CASE(PART)
                if (strcmp(me, nick) != 0) {
                        chanrep chan;
                        irc_chan(ctx, &chan, tokens[2]);
                        Buffer *b = chan.tag;
                        if (b != NULL) {
                                vec_push(
                                        b->messages,
                                        msg(
                                                "^<--^",
                                                "^%s^ (%s) has left %s: ",
                                                quit_color,
                                                ui_nick_color(nick),
                                                nick,
                                                tokens[0],
                                                b->name,
                                                tokens[3]
                                        )
                                );
                        }
                }
        END

        CASE(MODE)

        END

        CASE(NICK)
                if (strcmp(me, tokens[2]) == 0) {
                        set_nick_pattern(network, me);
                } else {
                        user_do_all_chans(ctx, tokens[2], ALL_CHANS_NICK, nick);
                }
        END

        CASE(PING)
                irc_printf(ctx, "PONG");
        END
#undef END
#undef CASE
}

static bool
h_quit(irc *ctx, tokarr *msg, size_t argc, bool pre)
{
        char const *ident = (*msg)[0];
        char const *reason = (*msg)[2];

        user_do_all_chans(ctx, ident, ALL_CHANS_QUIT, reason);

        return true;
}

inline static void
disconnected(Eria *state, Network *network)
{
}

inline static void
consume(Eria *state, Network *network)
{
        static tokarr tokens;
        irc *ctx = network->connection;

        for (;;) switch (irc_read(ctx, &tokens, 1)) {
        case  1: react(state, network, tokens); break;
        case  0:                                return;
        case -1:                                goto dead;
        }
dead:
        disconnected(state, network);
}

static Network *
network_new(char const *name, irc *connection)
{
        Network *n = alloc(sizeof *n);
        n->name = name;
        n->connection = connection;
        n->nick_regex = NULL;
        vec_init(n->buffers);
        vec_push(n->buffers, buffer_new(irc_get_host(connection), n, B_SERVER));
        set_nick_pattern(n, irc_get_nick(connection));
        return n;
}

static void
configure(Eria *state, Config *config)
{
        state->config = config;

        Network **networks = state->networks.items;

        for (NetworkConfig *net = config->networks; net->name != NULL; ++net) {
                irc *ctx = irc_init();
                if (ctx == NULL)
                        panic("out of memory");
                irc_set_server(ctx, net->server, net->port);
                irc_set_nick(ctx, net->nick);
                irc_set_uname(ctx, net->user);
                irc_set_fname(ctx, net->real);
                irc_set_connect_timeout(ctx, 0, net->timeout);
                irc_set_track(ctx, true);
                irc_reg_msghnd(ctx, "QUIT", h_quit, true);
                if (net->sasl.mechanism && net->sasl.key) {
                        irc_set_sasl(
                                ctx,
                                net->sasl.mechanism,
                                net->sasl.key,
                                strlen(net->sasl.key),
                                true
                        );
                }
                networks[state->networks.count++] = network_new(net->name, ctx);
        }
}

int
main(void)
{
        Eria state = { .fds = { { .fd = STDIN_FILENO, .events = POLLIN } } };
        configure(&state, &config);
        
        /*
        Network *network = state.networks.items[0];

        Room *r1 = room_new("room1", network, RM_SERVER);
        Room *r2 = room_new("room2", network, RM_USER);

        vec_push(network->rooms, r1);
        vec_push(network->rooms, r2);

        Message *m1 = message_new("Foo", "\x03""4Hello world");
        Message *m2 = message_new("Foobar", "                         dfsdkfb dhsbfhjdbs hjsdbf bhjbafjs bhjbasd : fsd fkn * ahsbdhjb asjdb abhjsdjabs d asjbd  Hi. \x03""4Hello world");

        vec_push(r1->messages, m1);
        vec_push(r2->messages, m1);

        vec_push(r1->messages, m2);
        vec_push(r2->messages, m2);
        */

        ui_init(&state);
        state.window = state.root;
        state.root->buffer = state.networks.items[0]->buffers.items[0];

        int flags = TERMKEY_FLAG_UTF8 | TERMKEY_CANON_DELBS | TERMKEY_FLAG_CTRLC;
        state.tk = termkey_new(STDIN_FILENO, flags);
        if (state.tk == NULL)
                panic("failed to construct TermKey instance");
        
        if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK, 1) == -1)
                epanic("fcntl()");
        
        ui_draw(&state);

        /* start connecting to all of our networks */
        pthread_t threads[ERIA_MAX_NETWORKS];
        for (int i = 0; i < state.networks.count; ++i) {
                irc *ctx = state.networks.items[i]->connection;
                /*
                int e = pthread_create(&threads[i], NULL, try_connect, ctx);
                if (e != 0)
                        panic("failed to spawn thread during startup: %s", strerror(e));
                        */
        }

        for (int i = 0; i < state.networks.count; ++i) {
                /*
                void *good;
                int e = pthread_join(threads[i], &good);
                if (e != 0)
                        panic("failed to join thread during startup: %s", strerror(e));
                if (!good)
                        fprintf(stderr, "warning: couldn't connect to %s:%hu\n",
                           config.networks[i].server, config.networks[i].port);
                           */
                irc_connect(state.networks.items[i]->connection);
        }

        /* autojoin channels */
        for (int i = 0; i < state.networks.count; ++i) {
                Network *network = state.networks.items[i];
                irc *ctx = network->connection;
                if (irc_online(ctx)) {
                        char const **chan = config.networks[i].channels;
                        while (*chan != NULL)
                                irc_printf(ctx, "JOIN :%s", *chan++);
                }
        }

        for (;;) {
                for (int i = 0; i < state.networks.count; ++i) {
                        irc *ctx = state.networks.items[i]->connection;
                        state.fds[1 + i].fd = irc_sockfd(ctx);
                        state.fds[1 + i].events = POLLIN;
                }

                int r = poll(state.fds, 1 + state.networks.count, -1);
                if (r == -1)
                        continue;

                /* check if there is anything on stdin */
                if (state.fds[0].revents & POLLIN) {
                        TermKeyKey input;
                        termkey_advisereadable(state.tk);
                        while (termkey_getkey(state.tk, &input) == TERMKEY_RES_KEY) {
                                static char key[64];
                                termkey_strfkey(state.tk, key, sizeof key, &input, TERMKEY_FORMAT_ALTISMETA);
                                if (input.type == TERMKEY_TYPE_UNICODE && input.modifiers == 0)
                                        handle_text(&state, input.utf8);
                                else
                                        handle_key(&state, key);

                        }
                }

                /* check for messages from the ircds we're connected to */
                static tokarr tokens;
                for (int i = 0; i < state.networks.count; ++i)
                        if (state.fds[1 + i].revents & POLLIN)
                                consume(&state, state.networks.items[i]);

                ui_draw(&state);
                state.window->buffer->activity = false;
        }

        return 0;
}
