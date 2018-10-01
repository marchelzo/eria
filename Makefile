CFLAGS  = -std=c11 -Wall -Wno-bool-operation -isystem /usr/local/opt/ncurses/include -isystem /usr/local/include -Wno-unused -fmax-errors=1 -Iinclude -I.
LDFLAGS = -L/usr/local/lib -ltermkey -lsrsirc -lm

ifdef RELEASE
	CFLAGS += -O3 -flto
else
	CFLAGS += -ggdb3 -fsanitize=address -fsanitize=undefined
endif

MAX_NETWORKS ?= 16
MAX_AUTOJOIN ?= 16

CFLAGS += -DERIA_MAX_NETWORKS=$(MAX_NETWORKS)
CFLAGS += -DERIA_MAX_AUTOJOIN=$(MAX_AUTOJOIN)

SOURCES = $(wildcard src/*.c)
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

%.o: %.c
	@echo cc $<
	@$(CC) $(CFLAGS) -c -o $@ $< $(LDFLAGS)

eria: $(OBJECTS)
	@echo cc $^
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) eria

