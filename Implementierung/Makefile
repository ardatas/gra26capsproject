all: julia

clean:
	rm julia

julia: main.c julia.c
	gcc -o julia main.c julia.c -lm