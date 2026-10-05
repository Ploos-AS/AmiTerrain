CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c99
CPPFLAGS ?= -Iinclude

LIBSRC = src/terrain.c src/io.c src/atf.c src/legacy.c

all: amiterrain

amiterrain: src/cli.c $(LIBSRC) include/amiterrain.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ src/cli.c $(LIBSRC)

test_core: tests/test_core.c $(LIBSRC) include/amiterrain.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_core.c $(LIBSRC)

test: test_core
	./test_core

clean:
	rm -f amiterrain test_core test-roundtrip.pgm test-roundtrip.atf

.PHONY: all test clean
