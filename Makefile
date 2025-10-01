all: server

server: server.c
	gcc -o server server.c -Wall -Wextra -pedantic

clean:
	rm -f server