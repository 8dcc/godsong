
CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -Wpedantic -ggdb3
LDLIBS=

#-------------------------------------------------------------------------------

.PHONY: all clean

all: song2pmx

clean:
	rm -f song2pmx

#-------------------------------------------------------------------------------

song2pmx: src/song2pmx.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
