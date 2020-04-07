/*
  Author: Braedon Wooding (z5204996)
 */

#ifndef __P2P_TCP_H__
#define __P2P_TCP_H__

#include "utils.h"

/**              **
 * Send a ping :) *
 **              **/

#define IP_ADDR ("127.0.0.1")

#define MAX_PENDING (3)

// The type of a tcp connection
typedef enum tcp_type_t {
  // joining the network
  TCP_JOIN_REQ,
  TCP_JOIN_RESP,
  TCP_PEER_DEPART,
  TCP_SUCC,
  TCP_STORE,
  TCP_RETRIEVE,
} tcp_type;

void *tcp_watcher(void *_ UNUSED_ATTR);

int tcp_send_join_req(int known_peer, int self);
int tcp_send_new_socket(int peer, char buf[]);
void tcp_send_quit_req(void);
int tcp_send_abrupt(int known, int left);

#endif
