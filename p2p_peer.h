/*
  Author: Braedon Wooding (z5204996)
 */

#ifndef __P2P_PEER_H__
#define __P2P_PEER_H__

#include <arpa/inet.h>

#define MIN_PEER_PORT (12000)
#define PEER_TO_PORT(peer) (MIN_PEER_PORT + peer)

typedef struct p2p_peer_info_t {
  int peer;
  int first_successor;
  int second_successor;
  int ping_interval;
} p2p_peer_info;

/*
  Get the ping interval for this p2p client.
 */
int get_ping_interval(void);

/*
  Get the peer # for this p2p client.
 */
int get_peer(void);

/*
  Get the first successor for this p2p client.
 */
int get_first_successor(int wait);

/*
  Get the second successor for this p2p client.
 */
int get_second_successor(int wait);

/*
  Clear the first successor for the p2p client.
  Used in conjunction with an eventual set_first_successor
  to support when we lose a successor.

  Returns old successor.
 */
int clear_first_successor(void);

/*
  Clear the second successor for the p2p client.
  Used in conjunction with an eventual set_second_successor
  to support when we lose a successor.

  Returns old successor.
 */
int clear_second_successor(void);

/*
  Set the first successor for the p2p client.
  Used in conjunction with an eventual clear_first_successor
  to support when we lose a successor.

  Fails and returns old successor if it wasn't cleared
 */
int set_first_successor(int next);

/*
  Set the second successor for the p2p client.
  Used in conjunction with an eventual clear_second_successor
  to support when we lose a successor.

  Fails and returns old successor if it wasn't cleared
 */
int set_second_successor(int next);

/*
  Clear / set both successors.
 */
void clear_and_set_successors(int first, int second);

void join_peer(int peer, int known, int ping,
                    pthread_t *ping_thrd, pthread_t *tcp_thrd);
void init_peer(int peer, int first, int second, int ping,
                    pthread_t *ping_thrd, pthread_t *tcp_thrd);

void close_peer(void);

void verify_peers();
pthread_t setup_ping_interval();

#endif