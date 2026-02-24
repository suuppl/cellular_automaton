run: output main
	./main

main: main.c
	cc -o main main.c

output:
	mkdir -p ./out