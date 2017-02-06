#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <libsrsirc/irc_track.h>
#include <libsrsirc/irc_ext.h>
#include <libsrsirc/util.h>
#include "eria.h"
#include "utf8.h"
#include "util.h"
#include "log.h"
#include "term.h"

#define MAX_NICK    15
#define TIME_LEN    8
#define LEFT_MARGIN (MAX_NICK + TIME_LEN + 1 + 3)

static Term term;

inline static double
hue_to_rgb(double p, double q, double t)
{
        if (t < 0)    t += 1;
        if (t > 1)    t -= 1;
        if (t < 1./6) return p + (q - p) * 6 * t;
        if (t < 1./2) return q;
        if (t < 2./3) return p + (q - p) * (2./3 - t) * 6;
        return p;
}

inline static Color
hsl_to_rgb(double h, double s, double l)
{
        double r, g, b;

        if (s == 0) {
                r = g = b = l;
        } else {
                double q = (l < 0.5) ? (l * (1 + s)) : (l + s - l * s);
                double p = 2 * l - q;
                r = hue_to_rgb(p, q, h + 1./3);
                g = hue_to_rgb(p, q, h);
                b = hue_to_rgb(p, q, h - 1./3);
        }

        return (Color) {
                .r = round(r * 255),
                .g = round(g * 255),
                .b = round(b * 255)
        };
}


Color
ui_nick_color(char const *nick)
{
        unsigned hash = 5381;

        for (int i = 0; nick[i] != '\0'; ++i)
                hash = (hash << 5) + hash + nick[i];
        
        srand(hash);
        
        double max = RAND_MAX;
        double h = (rand() / max);
        double s = (rand() / max) * 0.25 + 0.75;
        double l = (rand() / max) * 0.15 + 0.65;

        return hsl_to_rgb(h, s, l);
}

static Color
colorcode(char const **s)
{
        static Color const table[] = {
                { 1,   1,   1   },
                { 255, 255, 255 },
                { 0,   0,   0   },
                { 0,   0,   127 },
                { 0,   147, 0   },
                { 255, 0,   0   },
                { 127, 0,   0   },
                { 156, 0,   156 },
                { 252, 127, 0   },
                { 255, 255, 0   },
                { 0,   252, 0   },
                { 0,   147, 147 },
                { 0,   255, 255 },
                { 0,   0,   252 },
                { 255, 0,   255 },
                { 127, 127, 127 },
                { 210, 210, 210 },
        };

        if (**s == '#') {
                unsigned r, g, b;
                int n;
                sscanf(*s, "#%2x%2x%2x%n", &r, &g, &b, &n);
                *s += n;
                return (Color) { r, g, b };
        }

        if (!isdigit((*s)[0]))
                return C_DEFAULT;

        int i;
        if (isdigit((*s)[1]))
                i = 10 * ((*s)[0] - '0') + ((*s)[1] - '0');
        else
                i = (*s)[0] - '0';

        ++*s;
        if (isdigit(**s))
                ++*s;

        return table[(i + 1) % COUNTOF(table)];
}

static int
calcwidth(char const *s, int n)
{

        int width = 0;
        char const *end = s + n;
        int w;

        while (s < end) switch (*s++) {
        case 1:
                break;
        case 2:
                break;
        case 3:;
                Color fg = colorcode(&s);
                Color bg = (*s == ',') ? colorcode((++s, &s)) : C_DEFAULT;
                break;
        case 15:
                break;
        case 18:
                break;
        case 28:
                break;
        case 29:
                break;
        case 30:
                break;
        case 31:
                break;
        default:
                --s;
                char b[16];
                int n = utf8_next(s, &w);
                if (n == 0)
                        n = 1;
                memcpy(b, s, n);
                b[n] = '\0';
                width += w;
                s += n;
        }

        return width;
}

/* print with colors and stuff */
static void
drawtext(char const *s, int n, Video v)
{

        char const *end = s + n;

        Video dv = v;

        while (s < end) switch (*s++) {
        case 1:
                v.bold = false;
                break;
        case 2:
                v.bold = true;
                break;
        case 3:;
                Color fg = colorcode(&s);
                Color bg = (*s == ',') ? colorcode((++s, &s)) : dv.bg;
                if (bg.r == 1 && bg.g == 1 && bg.b == 1)
                        bg = dv.bg;
                v.fg = fg;
                v.bg = bg;
                break;
        case 15:
                v = dv;
                break;
        case 18:
                v.reverse = true;
                break;
        case 28:
                v.italic = false;
                break;
        case 29:
                v.italic = true;
                break;
        case 30:
                v.underline = false;
                break;
        case 31:
                v.underline = true;
                break;
        default:
                --s;
                char b[16];
                int n = utf8_next(s, &(int){0});
                if (n == 0)
                        n = 1;
                memcpy(b, s, n);
                b[n] = '\0';
                term_write(&term, v, b);
                s += n;
        }
}

static void
sigwinch(int s)
{
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
                term_resize(&term, ws.ws_row, ws.ws_col);
        } else {
                L("failed to get new terminal size");
        }
}

static int
draw_message(Window *w, Message *m, int row)
{
        static vec(int) blocks;
        blocks.count = 0;

        char const *body = m->body;
        int length = strlen(body);

        Color tc = { 60, 60, 60 };
        Color important = { 60, 60, 60 };

        while (length != 0) {
                int n = utf8_fit(body, length, w->width - LEFT_MARGIN);
                int i = n;
                while (n != length && i != 0 && body[i] != ' ')
                        --i;
                if (i == 0)
                        i = n;
                body += i;
                length -= i;
                vec_push(blocks, i);
                if (length != 0 && body[0] == ' ') {
                        ++body;
                        --length;
                }
        }

        Video v = V_NORMAL;

        int first_row = row - blocks.count + 1;
        if (first_row >= 0) {
                v.fg = tc;
                char tb[512];
                strftime(tb, sizeof tb, "%H:%M:%S", &m->time);
                term_mvprintf(&term, w->y + first_row, w->x, v, "%s ", tb);
                if (m->important) {
                        v.bold = true;
                        v.bg = important;
                }
                int pad = MAX_NICK - calcwidth(m->title, strlen(m->title));
                int n = strlen(m->title);
                if (n >= sizeof tb)
                        n = sizeof tb - 1;
                memcpy(tb, m->title, n);
                tb[n] = '\0';
                while (pad --> 0)
                        term_write(&term, v, " ");
                drawtext(tb, n, v);
                term_write(&term, v, " ");
                v = V_NORMAL;
        }

        body = m->body;
        for (int i = 0; i < blocks.count; ++i) {
                int n = blocks.items[i];
                int y = w->y + first_row + i;
                if (y >= w->y) {
                        term_mvprintf(&term, y, w->x + TIME_LEN + 1 + MAX_NICK + 1, v, "| ");
                        drawtext(body, n, V_NORMAL);
                }
                body += n;
                if (body[0] == ' ')
                        ++body;
        }

        return blocks.count;
}

static void
draw_rooms(Eria *state)
{
        Color fg = { 220, 220, 220 };
        Color bg = { 40, 40, 40 };
        Color activity = { 216, 139, 252 };
        Color current = { 80, 80, 80 };

        term_move(&term, 0, 0);
        int row = 1;

        Video v = V_NORMAL;
        v.fg = fg;
        v.bg = bg;

        for (int i = 0; i < state->networks.count; ++i) {
                Network *network = state->networks.items[i];
                for (int i = 0; i < network->buffers.count; ++i) {
                        Buffer *b = network->buffers.items[i];
                        if (b->activity)
                                v.fg = activity;
                        if (b == state->window->buffer)
                                v.bg = current;
                        if (b->type == B_SERVER)
                                term_mvprintf(&term, row++, 2, v, " %-24s", network->name);
                        else
                                term_mvprintf(&term, row++, 2, v, "     %-20s", b->name);
                        if (b->activity)
                                v.fg = fg;
                        if (b == state->window->buffer)
                                v.bg = bg;
                }
        }
}

static void
draw_window(Window *w)
{
        switch (w->type) {
        case W_VS:
        case W_HS:
                draw_window(w->one);
                draw_window(w->two);
                break;
        default:;
                Buffer *b = w->buffer;
                Network *network = b->network;
                irc *ctx = network->connection;
                char const *nick = irc_mynick(ctx);

                int row = w->height - 3;
                int mi = b->messages.count - (w->scroll + 1);

                while (mi >= 0 && row >= 0) {
                        row -= draw_message(w, b->messages.items[mi], row);
                        mi -= 1;
                }

                char status[512];
                switch (b->type) {
                case B_SERVER:
                        strcpy(status, irc_myhost(network->connection));
                        break;
                case B_CHANNEL:;
                        int users = irc_num_members(ctx, b->name);
                        snprintf(status, sizeof status, "%s@%s (%d user%s)", b->name, network->name, users, "s" + (users == 1));
                        break;
                case B_USER:
                        snprintf(status, sizeof status, "%s@%s", b->name, network->name);
                        break;
                default:
                        status[0] = '\0';
                }

                int n = strlen(status);
                int width = utf8_width(status, n);
                while (width++ < w->width)
                        status[n++] = ' ';
                status[n] = '\0';

                Video v = V_NORMAL;
                v.fg = (Color) { 240, 240, 240 };
                v.bg = (Color) { 50, 50, 50    };

                term_move(&term, w->y + w->height - 2, w->x);
                term_write(&term, v, status);

                static vec(char) input;
                input.count = 0;
                int cursor = 0;

                v = V_NORMAL;

                term_mvprintf(&term, w->y + w->height - 1, w->x, v, "[%s] ", nick);

                if (b->input.count != 0) {
                        for (int i = 0; i < b->input.count; ++i) {
                                if (b->input.items[i] != '\0')
                                        vec_push(input, b->input.items[i]);
                                if (i + 1 == b->cursor)
                                        cursor = input.count;
                        }

                        vec_push(input, '\0');

                        term_write(&term, v, input.items);
                        term_mvprintf(&term, w->y + w->height - 1, w->x, v, "[%s] ", nick);
                        input.items[cursor] = '\0';
                        term_write(&term, v, input.items);
                }
        }
}

void
ui_draw(Eria *state)
{
        term_clear(&term);
        draw_window(state->root);
        draw_window(state->window);
        term_flush(&term);
}

void
ui_init(Eria *state)
{
        sigwinch(SIGWINCH);
        signal(SIGWINCH, sigwinch);
        state->root = window_root(term.rows, term.cols);

        struct termios tp;
        tcgetattr(STDIN_FILENO, &tp);
        tp.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &tp);
}
