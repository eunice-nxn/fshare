all: fshared.c fshare.c
	gcc -o fshared fshared.c -pthread
	gcc -o fshare fshare.c
server: fshared.c
	gcc -o fshared fshared.c -pthread
client: fshare.c
	gcc -o fshare fshare.c
clean: fshared fshare
	rm fshared fshare
