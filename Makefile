CC = gcc
CFLAGS = -Wall

all: client server

client: TCPclient.c helpers.o
		$(CC) $(CFLAGS) TCPclient.c -o client helpers.o

server: TCPserver.c
		$(CC) $(CFLAGS) TCPserver.c -o server -pthread 

helpers.o: networkHelpers.c
		$(CC) $(CFLAGS) -c networkHelpers.c -o helpers.o

clean:
		rm *.o client server