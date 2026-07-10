all: clean julia

.PHONY: clean
clean:
	rm -f julia

julia: main.c julia.c utils.c
	gcc -o julia main.c julia.c utils.c -lm
