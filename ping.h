/*
  Author: Braedon Wooding (z5204996)
 */

#ifndef __P2P_INIT_H__
#define __P2P_INIT_H__

#include "p2p_peer.h"
#include "utils.h"

/**              **
 * Send a ping :) *
 **              **/

#define IP_ADDR ("127.0.0.1")

// The maximum number of ports we are sending
// pings to!
#define MAX_PING_FDS (2)

typedef enum ping_type_t {
  PING_ACK = 0,
  PING_REQ = 1,
} ping_type;

int send_pingfd(int ping_fd, ping_type type, int seq);

void drop_ping_info(int port);

int send_ping(char *ip, int port, ping_type type, int socket, int seq);

int initialise_ping_info(char *ip, int peer);

void *init_ping_module(void *_ UNUSED_ATTR);

void destroy_ping_module(void);

void *ping_thrd_ticker(void *_ UNUSED_ATTR);

int get_preds(int preds[MAX_PING_FDS]);

#endif
