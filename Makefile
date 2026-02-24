run: output main
	./main

main: main.c
	gcc $(CFLAGS) -o main main.c

output:
	mkdir -p ./out