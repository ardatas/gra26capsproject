all: clean julia

CC = gcc
CFLAGS = -std=c17 -O3 -msse4.2 -Wall -Wextra -pedantic
LDLIBS = -lm

.PHONY: clean
clean:
	rm -f julia

julia: main.c julia.c utils.c
	$(CC) $(CFLAGS) -o julia main.c julia.c utils.c $(LDLIBS)
