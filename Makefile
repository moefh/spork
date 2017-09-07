
CC = gcc
AR = ar rc
RANLIB = ranlib
CFLAGS = -Wall -Wextra -std=c99 -pedantic
LDFLAGS =
LIBS = -lm

CHECK_SCRIPT = tests/test.c

TARGETS = debug release ubsan

.PHONY: $(TARGETS) build clean check test dump_exported_symbols

all: debug

debug:
	$(MAKE) build TARGET_CFLAGS="-O1 -g" TARGET_LDFLAGS=""

release:
	$(MAKE) build TARGET_CFLAGS="-O2" TARGET_LDFLAGS="-s"

ubsan:
	$(MAKE) build TARGET_CFLAGS="-O1 -g -fsanitize=undefined" TARGET_LDFLAGS="-fsanitize=undefined"

clean:
	rm -f *~ tests/*~ core
	$(MAKE) -C src clean

build:
	$(MAKE) -C src CFLAGS="$(CFLAGS) $(TARGET_CFLAGS)" CC="$(CC)" LDFLAGS="$(LDFLAGS) $(TARGET_LDFLAGS)" LIBS="$(LIBS)" AR="$(AR)" RANLIB="$(RANLIB)"

check: debug
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all src/spork $(CHECK_SCRIPT)

dump_exported_symbols: debug
	nm src/lib/libspork.a | grep " [A-TV-Zuvw] "
