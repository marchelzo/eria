#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#define COUNTOF(a) (sizeof (a) / sizeof (a)[0])

char *
slurp(char const *path, size_t *n);

char *
sclone(char const *s);

#endif
