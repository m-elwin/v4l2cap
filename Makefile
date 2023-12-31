CC=gcc
CFLAGS=-std=gnu99 -g -Wall -Wextra

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

capture: capture.o
	$(CC) -o capture capture.o

.PHONY: clean

clean:
	rm -f capture.o capture
