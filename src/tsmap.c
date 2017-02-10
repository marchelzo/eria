/*
 * nick -> time they were last active in the channel
 * (used for nick completion priority)
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "vec.h"
#include "tsmap.h"

typedef struct {
        uintmax_t ts;
        uint32_t hash;
        char nick[52];
} Entry;

typedef vec(Entry) evec;

struct tsmap {
        evec es;
        evec alt;
        uintmax_t t;
        size_t used;
};

inline static uint32_t
hash(char const *s)
{
        uint32_t h = 5381;
        uint32_t c;

        while (c = *s++, c != '\0')
                h = (h << 5) + h + c;

        return h;
}

static void
put(evec *es, Entry const *e)
{
        uint32_t h = e->hash;
        unsigned i = h & (es->count - 1);

        while (es->items[i].nick[0] != '\0')
                i = (i + 1) & (es->count - 1);

        es->items[i] = *e;
}

static void
grow(TSMap *tsm)
{
        size_t sz = tsm->es.count * 2;

        tsm->alt.count = sz;
        vec_reserve(tsm->alt, sz);

        for (size_t i = 0; i < sz; ++i)
                tsm->alt.items[i].nick[0] = '\0';

        for (size_t i = 0; i < tsm->es.count; ++i)
                if (tsm->es.items[i].nick[0] != '\0')
                        put(&tsm->alt, &tsm->es.items[i]);

        evec tmp = tsm->es;
        tsm->es = tsm->alt;
        tsm->alt = tmp;
}

uintmax_t
tsmap_get(TSMap *tsm, char const *nick)
{
        uint32_t h = hash(nick);
        unsigned i = h & (tsm->es.count - 1);

        while (tsm->es.items[i].nick[0] != '\0')
                if (tsm->es.items[i].hash == h && strcmp(tsm->es.items[i].nick, nick) == 0)
                        return tsm->es.items[i].ts;
                else
                        i = (i + 1) & (tsm->es.count - 1);

        return 0;
}

uintmax_t
tsmap_update(TSMap *tsm, char const *nick)
{
        if (tsm->used * 2 > tsm->es.count)
                grow(tsm);

        uint32_t h = hash(nick);
        unsigned i = h & (tsm->es.count - 1);

        while (i < tsm->es.count && tsm->es.items[i].nick[0] != '\0')
                if (tsm->es.items[i].hash == h && strcmp(tsm->es.items[i].nick, nick) == 0)
                        goto End;
                else
                        i = (i + 1) & (tsm->es.count - 1);

        tsm->used += 1;
        tsm->es.items[i].hash = h;
        strcpy(tsm->es.items[i].nick, nick);
End:
        return tsm->es.items[i].ts = ++tsm->t;
}

TSMap *
tsmap_new(void)
{
        TSMap *tsm = alloc(sizeof *tsm);
        tsm->used = 0;
        tsm->t = 0;
        vec_init(tsm->es);
        vec_init(tsm->alt);

        tsm->es.count = 16;
        vec_reserve(tsm->es, tsm->es.count);

        for (size_t i = 0; i < tsm->es.count; ++i)
                tsm->es.items[i].nick[0] = '\0';

        return tsm;
}
