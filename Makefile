
CFLAGS=-g
CC=dcc

p2p: p2p_client.o utils.o
	$(CC) $(CFLAGS) -o p2p p2p_client.o utils.o
p2p_client.o: p2p_client.c
utils.o: utils.c

.PHONY : clean
clean:
	-rm p2p utils.o p2p_client.o
