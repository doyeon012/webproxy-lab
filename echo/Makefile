CC = gcc
CFLAGS = -O2 -Wall -I .
LIB = -lpthread

all: echoclient echoserver

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

echoclient: echo_client.c csapp.o
	$(CC) $(CFLAGS) -o echoclient echo_client.c csapp.o $(LIB)

echoserver: echo_server.c csapp.o
	$(CC) $(CFLAGS) -o echoserver echo_server.c csapp.o $(LIB)
        
clean:
	rm -f *.o echoclient echoserver *~




