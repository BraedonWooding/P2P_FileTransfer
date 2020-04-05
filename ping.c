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

    SCOPED_MTX_LOCK(&ping_lock) for (int i = 0; i < MAX_PING_FDS; i++) {
      time_t cur = time(NULL);
      char *ip = ping_rets[i].ip;
      int port = ping_rets[i].port;
      time_t next = ping_rets[i].next_ping_event;
      // printf("%d: %ld\n", i, next ? next - cur : 0);
      if (port && ip && next && next <= cur) {
        int diff = ping_rets[i].last_seq_sent - ping_rets[i].last_seq_received;
        if (diff >= PINGS_ABRUPT) {
          // they are abrupt
          printf("> Peer %d is no longer alive\n", port - MIN_PEER_PORT);
          // TODO: fix this...
          exit(1);
        } else {
          int seq = ++ping_rets[i].last_seq_sent;
          send_ping(ip, port, PING_REQ, send_socket, seq);
        }
      } else if (next && (!shortest || cur - next < shortest)) {
        // shorter time to wait upon
        shortest = cur - next;
      } else if (!next) {
        ping_rets[i].next_ping_event = cur + ping_interval;
      }
    }

    time_t diff = shortest ? shortest : ping_interval;
    // printf("%ld %ld\n", diff, shortest);
    // sleep till next send is available
    sleep(diff);
  }

  pthread_exit(NULL);
}

void *init_ping_module(void *_) {
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
  ssize_t bytes_served = 0;
  while (count > 0 || !bytes_served) {
    count = sendto(socket, buf + bytes_served, strlen(buf) - bytes_served, 0,
                  (struct sockaddr *)&ping_out, sizeof(ping_out));
    if (count >= 0) bytes_served += count;
  }
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
    count = recvfrom(read_socket, buf, BUF_LEN, 0, (struct sockaddr *)&in, &len);
    READ_MSG_TYPE(0, buf, " ");
    int seq = READ_MSG_POSINT(0);
    int peer = READ_MSG_POSINT(0);

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
      printf("> Ping request received from Peer %d\n", port - MIN_PEER_PORT);
      int swp = ping_preds[0];
      ping_preds[0] = port - MIN_PEER_PORT;

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
    if (ping_preds[i]) preds[count++] = ping_preds[i];
  }
  return count;
}

int drop_ping_info(int ping_fd) {
  return 0;
}

int initialise_ping_info(char *ip, int peer) {
  SCOPED_MTX_LOCK(&ping_lock) for (int i = 0; i < MAX_PING_FDS; i++) {
    if (!ping_rets[i].port) {
      // free spot
      ping_rets[i] = (ping_info){.ip = ip, .port = peer + MIN_PEER_PORT};
      return i;
    }
  }

  return -1;
}
