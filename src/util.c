#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "panic.h"
#include "alloc.h"
#include "log.h"

char *
slurp(char const *path, size_t *n)
{
        int fd = open(path, O_RDONLY);
        if (fd == -1)
                epanic("open(%s)", path);

        off_t size = lseek(fd, 0, SEEK_END);
        if (size == -1)
                epanic("lseek()");

        lseek(fd, 0, SEEK_SET);

        char *data = alloc(size + 1);

        if (read(fd, data, size) != size)
                epanic("read()");

        data[size] = '\0';
        close(fd);

        if (n != NULL)
                *n = size;

        return data;
}

char *
sclone(char const *s)
{
        char *new = alloc(strlen(s) + 1);
        return strcpy(new, s);
}
