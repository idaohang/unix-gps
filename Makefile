all: pojazd server client
server: server.o lib.o
	gcc -pthread -Wall -o server server.o lib.o -lm
pojazd: pojazd.o lib.o
	gcc -pthread -Wall -o pojazd pojazd.o lib.o -lm
client: client.o lib.o
	gcc -pthread -Wall -o client client.o lib.o -lm
server.o: server.c
	gcc -c -Wall -o server.o server.c
pojazd.o: pojazd.c
	gcc -c -Wall -o pojazd.o pojazd.c
client.o: client.c
	gcc -c -Wall -o client.o client.c
lib.o: lib.c
	gcc -c -Wall -o lib.o lib.c

.PHONY: all clean

clean:
	rm -f *o pojazd server client