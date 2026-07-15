# Renamed binary target to "project"
# Edited by Arda Tas on 11.07.2026
CC = gcc
CFLAGS = -std=c17 -O3 -g -msse4.2 -Wall -Wextra -pedantic
LDLIBS = -lm

TARGET = project
SOURCES = main.c julia.c utils.c 

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) julia.h utils.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LDLIBS)

clean:
	rm -f $(TARGET)