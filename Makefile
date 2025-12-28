
CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -Wpedantic -ggdb3
LDLIBS=

#-------------------------------------------------------------------------------

.PHONY: all clean

all: godsong godsong2pmx godsong2lilypond

clean:
	rm -f godsong godsong2pmx godsong2lilypond

godsong: src/generators/godsong.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

godsong2pmx: src/converters/godsong2pmx.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

godsong2lilypond: src/converters/godsong2lilypond.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

