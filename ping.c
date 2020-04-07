#include "ping.h"

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "utils.h"
#include "tcp.h"
#include "p2p_peer.h"

typedef struct ping_info_t {
  // the IP we are sending to
  // required ownership!
  char *ip;

  // the port we are sending to
  int port;

  // the number we have sent out
  int last_seq_sent;

  // the number we have gotten back
  int last_seq_received;

  // the time of the next ping event
  time_t next_ping_event;
} ping_info;

static ping_info ping_rets[MAX_PING_FDS] = {};
// we'll remember the last two pings we got
// as what predecessors we have!
// This does mean that when we depart we may need
// to wait for this to be full (i.e. if we depart
// very very quickly then we'll have to wait for more pings)
static int ping_preds[MAX_PING_FDS] = {};

static pthread_mutex_t ping_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ping_wait = PTHREAD_COND_INITIALIZER;
static int send_socket = -1;
static int read_socket;

#define BUF_LEN (2048)

#define PING_MSG(x) (#x)
#define PING_BUF (25)
#define PINGS_ABRUPT (3)

static void ping_receiver_thread();

void *ping_thrd_ticker(void *_ UNUSED_ATTR) {
  // not currently modifiable in program
  // to reduce contention just cache
  int ping_interval = get_ping_interval();

  for (;;) {
    time_t shortest = 0;
    int abrupt = -1;

    SCOPED_MTX_LOCK(&ping_lock) for (int i = 0; i < MAX_PING_FDS; i++) {
      time_t cur = time(NULL);
      char *ip = ping_rets[i].ip;
      int port = ping_rets[i].port;
      time_t next = ping_rets[i].next_ping_event;
      if (port && ip && next && next <= cur) {
        int diff = ping_rets[i].last_seq_sent - ping_rets[i].last_seq_received;
        if (diff >= PINGS_ABRUPT) {
          // they are abrupt
          printf("> Peer %d is no longer alive\n", port - MIN_PEER_PORT);
          abrupt = i;
          break;
        } else {
          int seq = ++ping_rets[i].last_seq_sent;
          send_ping(ip, port, PING_REQ, send_socket, seq);
        }
        ping_rets[i].next_ping_event = cur + ping_interval;
        if (!shortest) shortest = ping_interval;
      } else if (next && (!shortest || cur - next < shortest)) {
        // shorter time to wait upon
        shortest = cur - next;
      } else if (!next) {
        ping_rets[i].next_ping_event = cur + ping_interval;
        if (!shortest) shortest = ping_interval;
      }
    }

    switch (abrupt) {
      case 0: {
        int left = clear_first_successor();

        // this is explicitly non blocking...
        // we don't want them to go looking for their second
        // during the first, if they don't have a second then
        // that is a big problem.
        int second = get_second_successor(0);
        if (second == -1) {
          fprintf(stderr, "Error: Peer %d has lost it's first successor"
                  " and has no second successor exiting...\n", get_peer());
          // We don't have to send a leave request because what data would we
          // send them... both our successors are invalidated!
          exit(1);
        }

        int new = tcp_send_abrupt(second, left);
        if (new < 0) {
          // This can only really happen if the other successor drops
          // in which we again can't actually handle in a nice manner
          // so we'll just exit
          // (there is no ability for us to grab a successor if this fails)
          fprintf(stderr, "Error: Got invalid successor talking to %d exiting...\n", second);
          exit(1);
        }
        printf("> My new first successor is Peer %d\n", second);
        printf("> My new second successor is Peer %d\n", new);
        clear_and_set_successors(second, new);
      } break;
      case 1: {
        int left = clear_second_successor();

        // this is explicitly non blocking...
        // we don't want them to go looking for their second
        // during the first, if they don't have a second then
        // that is a big problem.
        int first = get_first_successor(0);
        if (first == -1) {
          fprintf(stderr, "Error: Peer %d has lost it's second successor"
                  " and has no first successor exiting...\n", get_peer());
          // We don't have to send a leave request because what data would we
          // send them... both our successors are invalidated!
          exit(1);
        }

        int new = tcp_send_abrupt(first, left);
        if (new < 0) {
          // This can only really happen if the other successor drops
          // in which we again can't actually handle in a nice manner
          // so we'll just exit
          // (there is no ability for us to grab a successor if this fails)
          fprintf(stderr, "Error: Got invalid successor talking to %d exiting...\n", first);
          exit(1);
        }
        printf("> My new first successor is Peer %d\n", first);
        printf("> My new second successor is Peer %d\n", new);
        clear_and_set_successors(first, new);
      } break;
    }

    if (!shortest) {
      SCOPED_MTX_LOCK(&ping_lock) {
        // we require atleast one object to be initialised
        while (!ping_rets[0].port) pthread_cond_wait(&ping_wait, &ping_lock);
      }
      continue;
    }

    // sleep till next send is available
    sleep(shortest);
  }

  pthread_exit(NULL);
}

void *init_ping_module(void *_) {
  memset(ping_preds, -1, sizeof(int) * MAX_PING_FDS);

  // for recv'ing pings
  read_socket = socket(AF_INET, SOCK_DGRAM, 0);

  struct sockaddr_in bind_addr;
  set_sockaddr(&bind_addr, IP_ADDR, get_peer() + MIN_PEER_PORT);
  setsockopt(read_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
  bind(read_socket, (struct sockaddr *)&bind_addr, sizeof(bind_addr));

  // move control to receiver thread
  ping_receiver_thread();
  pthread_exit(NULL);
}

void destroy_ping_module(void) {
  close(send_socket);
  close(read_socket);
}

int send_ping(char *ip, int port, ping_type type, int socket, int seq) {
  struct sockaddr_in ping_out;
  set_sockaddr(&ping_out, ip, port);
  char buf[PING_BUF];
  switch (type) {
    case PING_ACK: {
      printf("> Ping response sent to %d\n", port - MIN_PEER_PORT);
      snprintf(buf, PING_BUF, "%s %d %d", PING_MSG(PING_ACK), seq, get_peer());
    } break;
    case PING_REQ: {
      printf("> Ping request sent to %d\n", port - MIN_PEER_PORT);
      snprintf(buf, PING_BUF, "%s %d %d", PING_MSG(PING_REQ), seq, get_peer());
    } break;
    default: {
      fprintf(stderr, "Valid Ping types are %d and %d\n", PING_ACK, PING_REQ);
      fprintf(stderr, "[Error]: Invalid Ping Type: %d\n", type);
      errno = EINVAL;
      return -1;
    }
  }

  ssize_t count = 0;
  count = sendto(socket, buf, strlen(buf), 0,
                (struct sockaddr *)&ping_out, sizeof(ping_out));
  return count >= 0 ? strlen(buf) - count : -1;
}

int send_pingfd(int ping_fd, ping_type type, int seq) {
  if (send_socket == -1) send_socket = socket(AF_INET, SOCK_DGRAM, 0);

  int port = 0;
  char *ip = NULL;

  SCOPED_MTX_LOCK(&ping_lock) if (0 <= ping_fd && ping_fd < MAX_PING_FDS) {
    port = ping_rets[ping_fd].port;
    ip = ping_rets[ping_fd].ip;
  }

  return port && ip ? send_ping(ip, port, type, send_socket, seq) : -1;
}

// A thread responsible for receiving pings
void ping_receiver_thread() {
  struct sockaddr_in in;
  socklen_t len;
  ssize_t count;
  char buf[BUF_LEN] = {};
  char ip[INET_ADDRSTRLEN] = {};
  int err;
  int port;

  for (;;) {
    len = sizeof(in);
    memset(&in, 0, len);
    count = recvfrom(read_socket, buf, PING_BUF, 0, (struct sockaddr *)&in, &len);
    if (count == 0) break;

    buf[count] = '\0';
    READ_MSG_TYPE(0, buf, " ");
    int seq = READ_MSG_POSINT(0);
    int peer = READ_MSG_POSINT(0);
    if (seq < 0 || peer < 0) {
      // not a valid request this is fine
      break;
    }

    port = peer + MIN_PEER_PORT;
    if (!strcmp(buf, PING_MSG(PING_ACK))) {
      SCOPED_MTX_LOCK(&ping_lock) {
        int i = 0;
        for (; i < MAX_PING_FDS; i++) {
          if (ping_rets[i].port == port) {
            break;
          }
        }

        if (i < MAX_PING_FDS) {
          // successor
          if (seq > ping_rets[i].last_seq_received) {
            ping_rets[i].last_seq_received = seq;
          }
          printf("> Ping response received from Peer %d\n", peer);
        } else {
          printf("> Ping response received from Peer %d but wasn't expecting it\n",
                 port - MIN_PEER_PORT);
        }
      }
    } else if (!strcmp(buf, PING_MSG(PING_REQ))) {
      // we'll send back an acknowledgement
      printf("> Ping request received from Peer %d\n", peer);
      int swp = ping_preds[0];
      ping_preds[0] = peer;

      SCOPED_MTX_LOCK(&ping_lock) for (int i = 1; swp && i < MAX_PING_FDS; i++){
        int tmp = ping_preds[i];
        ping_preds[i] = swp;
        swp = ping_preds[i];
      }

      inet_ntop(in.sin_family, &in.sin_addr, ip, INET_ADDRSTRLEN);
      // we don't update our 'sent' seq for this...
      // since this is just an ack we don't acknowledge that we sent
      // the initial request.
      err = send_ping(ip, port, PING_ACK, read_socket, seq);

      if (err) {
        const char *msg = err < 0 ? "" : "Failed to send ";
        fprintf(stderr, "[Error]: Ping to %s:%d failed due to ", ip, port);
        if (err < 0) {
          fprintf(stderr, "%s\n", strerror(errno));
        } else {
          fprintf(stderr, "not sending all the bytes %d were remaining\n", err);
        }
      }
    } else {
      fprintf(stderr, "[Error]: Ignoring non ping msg %s\n", buf);
    }
  }
}

int get_preds(int preds[MAX_PING_FDS]) {
  int count = 0;
  SCOPED_MTX_LOCK(&ping_lock) for (int i = 0; i < MAX_PING_FDS; i++) {
    preds[count++] = ping_preds[i];
  }
  return count;
}

void drop_ping_info(int port) {
  SCOPED_MTX_LOCK(&ping_lock) for (int i = 0; i < MAX_PING_FDS; i++) {
    if (ping_rets[i].port == port) {
      // free spot
      ping_rets[i] = (ping_info){};
      break;
    }
  }
}

int initialise_ping_info(char *ip, int peer) {
  SCOPED_MTX_LOCK(&ping_lock) for (int i = 0; i < MAX_PING_FDS; i++) {
    if (!ping_rets[i].port) {
      // free spot
      ping_rets[i] = (ping_info){.ip = ip, .port = peer + MIN_PEER_PORT};
      pthread_cond_broadcast(&ping_wait);
      return i;
    }
  }

  return -1;
}
