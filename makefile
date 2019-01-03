.PHONY:clean all
CC=gcc
CFLAGS=-Wall -g









BIN=echoserver echoclient echoserver_select p2pserver p2pclient
all:$(BIN)
%.o:%.c
	$(CC) $(CFLAGS) _-c_$< -o $@
clean:
	rm -f *.o $(BIN)
