#ifndef TSMAP_H_INCLUDED
#define TSMAP_H_INCLUDED

#include <stdint.h>

struct tsmap;
typedef struct tsmap TSMap;

uintmax_t
tsmap_get(TSMap *tsm, char const *nick);

uintmax_t
tsmap_update(TSMap *tsm, char const *nick);

TSMap *
tsmap_new(void);

#endif
