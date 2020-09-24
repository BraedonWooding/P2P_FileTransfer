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
  // Client attemping to join network
  // data: int peer
  TCP_JOIN_REQ,

  // Response to client attempting to join
  // contains their successors
  // data: int first, second
  TCP_JOIN_RESP,

  // Peer departing network
  // data: int peer, first, second
  TCP_PEER_DEPART,

  // Abrupt departure peer wants list of successors
  // data: int peer, peer_left;
  TCP_SUCC,

  // Attempt to retrieve a file upon finding peer it'll initialise
  // a TCP_TRANSFER (of type SEND) and send the file across.
  // data: int file, int peer_requesting
  TCP_RETRIEVE,

  // Attempt to store a file upon finding peer it'll initialise
  // a TCP_TRANSFER (of type REQUEST) and read the file in.
  // data: int file, int peer_requesting
  TCP_STORE,

  // Perform a transfer given the correct type will send
  // data: int file_id
  // Will prefix the file with a NULL terminated file name.
  TCP_TRANSFER,
} tcp_type;

/*
  Watch for new connections
*/
void *tcp_watcher(void *_ UNUSED_ATTR);

/*
  Send a join request using a known peer.
*/
int tcp_send_join_req(int known_peer, int self);

/*
  Send a join request using a known peer.
*/
int tcp_send_new_socket(int peer, char buf[]);

/*
  Send a quit request.
*/
void tcp_send_quit_req(void);

/*
  Send detected abrupt request asking for new successors.
*/
int tcp_send_abrupt(int known, int left);

/*
  Send a retrieve / request 'request' asking for all files with id given.
*/
int tcp_send_retrieve_req(int file, int peer_requesting, int peer);

/*
  Send a store 'request' asking to store a given file.
*/
int tcp_send_store_req(int file, int peer_requesting, int peer);

/*
  Send a file with a specific extension to a peer.
*/
void tcp_transfer_send(int file, char *ext, int peer);

#endif
