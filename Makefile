all: pojazd server
server: server.o lib.o
	gcc -pthread -Wall -o server server.o lib.o
pojazd: pojazd.o lib.o
	gcc -pthread -Wall -o pojazd pojazd.o lib.o
server.o: server.c
	gcc -c -Wall -o server.o server.c
pojazd.o: pojazd.c
	gcc -c -Wall -o pojazd.o pojazd.c
lib.o: lib.c
	gcc -c -Wall -o lib.o lib.c

.PHONY: all clean

clean:
	rm -f *o
	rm -f pojazd
	rm -f server