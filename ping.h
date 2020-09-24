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

/*
  Send a ping using a ping descriptor.
*/
int send_pingfd(int ping_fd, ping_type type, int seq);

/*
  Drops successor information beloing to port from the ping module.
*/
void drop_ping_info(int port);

/*
  Send a ping using the IP and port of the dest and a supplied socket.
*/
int send_ping(char *ip, int port, ping_type type, int socket, int seq);

/*
  Reinitialises successor using given IP and peer value.
*/
int initialise_ping_info(char *ip, int peer);

/*
  Initialises the ping module!
*/
void *init_ping_module(void *_ UNUSED_ATTR);

/*
  Closes all sockets and deinitialises memory.
*/
void destroy_ping_module(void);

/*
  The thread for a ping ticker.  Must be initialised separately to the module
*/
void *ping_thrd_ticker(void *_ UNUSED_ATTR);

/*
  Get all predecessors read returns count of preds.
*/
int get_preds(int preds[MAX_PING_FDS]);

#endif
