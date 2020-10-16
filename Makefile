CC:=gcc
CFLAGS:= -lssl -lcrypto -o request

all:
	$(CC) *.c $(CFLAGS) -o request