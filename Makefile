all: func-overview

SHARED = ft-reader.c ft-reader.h

PKGS = gobject-2.0

func-overview: $(SHARED) func-overview.c
	$(CC) -o $@ -g -Wall -Werror func-overview.c $(SHARED) $(shell pkg-config --libs --cflags $(PKGS)) -lm

clean:
	rm -rf func-overview
