#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <libsrsirc/irc_track.h>
#include <libsrsirc/util.h>
#include "util.h"
#include "buffer.h"
#include "eria.h"
#include "vec.h"
#include "ui.h"
#include "log.h"

typedef void (Action)(Eria *);

static void
quit(Eria *state)
{
        for (int i = 0; i < state->networks.count; ++i) {
                Network *network = state->networks.items[i];
                irc_printf(network->connection, "QUIT");
        }

        exit(EXIT_SUCCESS);
}

static void
backspace(Eria *state)
{
        int cursor = state->window->buffer->cursor;
        char *input = state->window->buffer->input.items;
        int bytes = state->window->buffer->input.count;

        if (cursor == 0)
                return;

        int i = cursor - 1;
        while (i > 0 && input[i - 1] != '\0')
                --i;

        memmove(input + i, input + cursor, bytes - cursor);
        state->window->buffer->input.count -= cursor - i;
        state->window->buffer->cursor = i;
}

static void
left(Eria *state)
{
        char const *input = state->window->buffer->input.items;
        int *cursor = &state->window->buffer->cursor;

        if (*cursor == 0)
                return;

        while (--*cursor != 0 && input[*cursor - 1] != '\0')
                ;
}

static void
right(Eria *state)
{
        char const *input = state->window->buffer->input.items;
        int *cursor = &state->window->buffer->cursor;

        if (*cursor == state->window->buffer->input.count)
                return;

        *cursor += strlen(input + *cursor) + 1;
}

static void
goto_start(Eria *state)
{
        state->window->buffer->cursor = 0;
}

static void
goto_end(Eria *state)
{
        state->window->buffer->cursor = state->window->buffer->input.count;
}

static void
cut_rest(Eria *state)
{
        state->window->buffer->input.count = state->window->buffer->cursor;
}

static void
enter(Eria *state)
{
        int n = 0;
        char *input = state->window->buffer->input.items;

        if (state->window->buffer->input.count == 0)
                return;

        for (int i = 0; i < state->window->buffer->input.count; ++i)
                if (input[i] != '\0')
                        input[n++] = input[i];
        input[n] = '\0';

        Buffer *buffer = state->window->buffer;
        Network *network = buffer->network;
        irc *ctx = network->connection;
        char const *nick = irc_mynick(ctx);

        if (buffer == network->buffers.items[0])
                irc_printf(ctx, "%s", input);
        else
                irc_printf(ctx, "PRIVMSG %s :%s", state->window->buffer->name, input);

        Message *m = msg("^%s^", "%s", ui_nick_color(nick), nick, input);
        vec_push(buffer->messages, m);

        buffer->input.count = 0;
        buffer->cursor = 0;
}

static void
prev_buffer(Eria *state)
{
        Buffer *buffer = state->window->buffer;
        Network *network = buffer->network;

        if (network == state->networks.items[0] && buffer == network->buffers.items[0]) {
                network = *vec_last(state->networks);
                state->window->buffer = *vec_last(network->buffers);
        } else if (buffer == network->buffers.items[0]) {
                Network **n = state->networks.items;
                while (n[1] != network)
                        ++n;
                state->window->buffer = *vec_last(n[0]->buffers);
        } else {
                Buffer **r = network->buffers.items;
                while (r[1] != buffer)
                        ++r;
                state->window->buffer = r[0];
        }
}

static void
next_buffer(Eria *state)
{
        Buffer *buffer = state->window->buffer;
        Network *network = buffer->network;

        if (network == *vec_last(state->networks) && buffer == *vec_last(network->buffers)) {
                network = state->networks.items[0];
                state->window->buffer = network->buffers.items[0];
        } else if (buffer == *vec_last(network->buffers)) {
                Network **n = state->networks.items;
                while (n[0] != network)
                        ++n;
                state->window->buffer = n[1]->buffers.items[0];
        } else {
                Buffer **r = network->buffers.items;
                while (r[0] != buffer)
                        ++r;
                state->window->buffer = r[1];
        }
}

static void
scroll_up(Eria *state)
{
        Window *window = state->window;
        window->scroll += 5;
        if (window->scroll > window->buffer->messages.count)
                window->scroll = window->buffer->messages.count;
}

static void
scroll_down(Eria *state)
{
        Window *window = state->window;
        window->scroll -= 5;
        if (window->scroll < 0)
                window->scroll = 0;
}

static void
hsplit(Eria *state)
{
        window_hsplit(state->window, state->window->buffer, -1);
        state->window = state->window->top;
}

static void
vsplit(Eria *state)
{
        window_vsplit(state->window, state->window->buffer, -1);
        state->window = state->window->left;
}

static void
close_window(Eria *state)
{
        state->window = window_delete(state->window);
}

static void
resize_window(Eria *state)
{
        --state->resize;
}

static void
wdown(Eria *state)
{
        if (state->resize) {
                if (state->window->parent == NULL)
                        return;
                window_grow_y(state->window, -1);
        } else {
                state->window = window_down(state->window);
        }
}

static void
wleft(Eria *state)
{
        if (state->resize) {
                if (state->window->parent == NULL)
                        return;
                window_grow_x(state->window, -1);
        } else {
                state->window = window_left(state->window);
        }
}

static void
wright(Eria *state)
{
        if (state->resize) {
                if (state->window->parent == NULL)
                        return;
                window_grow_x(state->window, 1);
        } else {
                state->window = window_right(state->window);
        }
}

static void
wup(Eria *state)
{
        if (state->resize) {
                if (state->window->parent == NULL)
                        return;
                window_grow_y(state->window, 1);
        } else {
                state->window = window_up(state->window);
        }
}

static void
replace_buffer(Window *w, Buffer *old, Buffer *new)
{
        switch (w->type) {
        case W_VS:
        case W_HS:
                replace_buffer(w->one, old, new);
                replace_buffer(w->two, old, new);
                break;
        case W_W:
                if (w->buffer == old)
                        w->buffer = new;
                break;
        }
}

static void
leave_buffer(Eria *state)
{
        Buffer *buffer = state->window->buffer;
        Network *network = buffer->network;
        irc *ctx = network->connection;

        if (buffer->type == B_SERVER)
                return;

        if (buffer->type == B_CHANNEL)
                irc_printf(ctx, "PART %s: %s", buffer->name, "Leaving");

        int i = 1;
        while (network->buffers.items[i] != buffer)
                ++i;

        replace_buffer(state->root, state->window->buffer, network->buffers.items[i - 1]);

        memmove(
                network->buffers.items + i,
                network->buffers.items + i + 1,
                (network->buffers.count - i - 1) * sizeof (Buffer)
        );

        --network->buffers.count;
}

static void
complete(Eria *state)
{
        Buffer *buffer = state->window->buffer;
        Network *network = buffer->network;
        irc *ctx = network->connection;
        static int i;
        static int start;

        if (state->complete_again) {
                if (i != -1) {
                        backspace(state);
                        if (start == 0)
                                backspace(state);
                }
                goto next;
        }

        /* for now we just complete nicks in channels */
        if (buffer->type != B_CHANNEL)
                return;

        static char prefix[64];
        static char *p;
        p = prefix;
        char *end = p + sizeof prefix - 1;
        char *input = buffer->input.items;

        int c = buffer->cursor;
        while (c > 0 && !isspace(input[c - 1]))
                --c;

        start = (c == 0) ? 0 : c + 1;

        while (p < end && c < buffer->cursor)
                if (input[c++] != '\0')
                        *p++ = input[c - 1];
        *p = '\0';

        if (p == prefix)
                return;

        static vec(userrep) members;
        static int n;
        static int len;

        n = irc_num_members(ctx, buffer->name);
        len = strlen(prefix);
        vec_reserve(members, irc_num_members(ctx, buffer->name));
        irc_all_members(ctx, buffer->name, members.items, n);
        i = -1;
        state->complete_again = true;
next:
        for (++i; i < n; ++i) {
                char const *nick = members.items[i].nick;
                if (lsi_ut_istrncmp(prefix, nick, len, 0) == 0) {
                        while (buffer->cursor != start)
                                backspace(state);
                        int cl = strlen(nick) + 1;
                        vec_insert_n(buffer->input, nick, cl, buffer->cursor);
                        buffer->cursor += cl;
                        if (start == 0) {
                                vec_insert_n(buffer->input, ": ", 3, buffer->cursor);
                                buffer->cursor += 3;
                        }
                        return;
                }
        }

        i = -1;
}

static void
jump_active(Eria *state)
{
        Buffer *save = state->window->buffer;

        do {
                next_buffer(state);
        } while (!state->window->buffer->activity && state->window->buffer != save);
}

static void
jump_server(Eria *state)
{
        state->window->buffer = state->window->buffer->network->buffers.items[0];
}

struct {
        char const *key;
        Action *action;
} keys[] = {
        /* has to be sorted for binary search */
        { "Backspace", backspace     },
        { "C-a",       goto_start    },
        { "C-c",       quit          },
        { "C-e",       goto_end      },
        { "C-k",       cut_rest      },
        { "C-n",       jump_active   },
        { "C-q",       hsplit        },
        { "C-r",       resize_window },
        { "C-s",       jump_server   },
        { "C-v",       vsplit        },
        { "C-w",       leave_buffer  },
        { "C-x",       close_window  },
        { "DEL",       backspace     },
        { "Enter",     enter         },
        { "Left",      left          },
        { "M-Left",    prev_buffer   },
        { "M-Right",   next_buffer   },
        { "PageDown",  scroll_down   },
        { "PageUp",    scroll_up     },
        { "Right",     right         },
        { "S-Down",    wdown         },
        { "S-Left",    wleft         },
        { "S-Right",   wright        },
        { "S-Up",      wup           },
        { "Tab",       complete      },
};

static Action *
lookup(char const *key)
{
        int lo = 0;
        int hi = COUNTOF(keys) - 1;

        while (lo <= hi) {
                int m = (lo + hi) / 2;
                int o = strcmp(key, keys[m].key);
                if (o < 0)  hi = m - 1;
                if (o > 0)  lo = m + 1;
                if (o == 0) return keys[m].action;
        }

        return NULL;
}

void
handle_key(Eria *state, char const *s)
{
        Action *a = lookup(s);
        if (a != NULL)
                a(state);
        if (a != complete)
                state->complete_again = false;
}

void
handle_text(Eria *state, char const *s)
{
        int n = strlen(s) + 1;
        int i = state->window->buffer->cursor;
        vec_insert_n(state->window->buffer->input, s, n, i);
        state->window->buffer->cursor += n;
        state->complete_again = false;
}
