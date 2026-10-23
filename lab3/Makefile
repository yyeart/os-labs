.PHONY: all clean

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -pthread

all: filter

filter: src/main.c src/matrix.c src/filter.c src/matrix.h src/filter.h
	$(CC) $(CFLAGS) src/main.c src/matrix.c src/filter.c -o filter

clean:
	rm -f filter