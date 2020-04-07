
# We link against pthread to get access to ISO C threads
# Mild performance selection of O2.
CFLAGS=-pthread -std=c11 -O2
CC=gcc

p2p: entry.o utils.o ping.o p2p_peer.o tcp.o
	$(CC) $(CFLAGS) -o p2p entry.o utils.o ping.o p2p_peer.o tcp.o
entry.o: entry.c
utils.o: utils.c
ping.o: ping.c
p2p_peer.o: p2p_peer.c
tcp.o: tcp.c

.PHONY : clean
clean:
	-rm p2p entry.o utils.o ping.o p2p_peer.o tcp.o