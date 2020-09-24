#include "p2p_peer.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ping.h"
#include "utils.h"
#include "tcp.h"

static p2p_peer_info info = {
  .first_successor = -1, .second_successor = -1, .peer = -1
};
static pthread_mutex_t info_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t info_wait = PTHREAD_COND_INITIALIZER;

int init_peer(int peer, int first, int second, int ping,
                    pthread_t *ping_thrd, pthread_t *tcp_thrd) {
  info.peer = peer;
  info.first_successor = first;
  info.second_successor = second;
  info.ping_interval = ping;

  printf("> Peer %d init\n", peer);

  pthread_create(ping_thrd, NULL, init_ping_module, NULL);
  pthread_create(tcp_thrd, NULL, tcp_watcher, NULL);
  return 0;
}

int join_peer(int peer, int known, int ping, pthread_t *ping_thrd,
                    pthread_t *tcp_thrd) {
  info.peer = peer;
  info.ping_interval = ping;
  printf("> Peer %d join\n", peer);

  // we still want to be able to send pings responses out
  // i.e. if we have 9 -> 14 -> 16 and we inserting 15
  // then 9 may get updated second successor (15) before it's
  // actually finished initialising so we need to allow it to
  // be able to send back the ping response!!
  pthread_create(ping_thrd, NULL, init_ping_module, NULL);
  pthread_create(tcp_thrd, NULL, tcp_watcher, NULL);

  tcp_send_join_req(known, peer);

  // and the tcp watcher will just cancel all responses till we get our data
  // note: we probably want to make this a condition variable wait...
  return 0;
}

int get_ping_interval(void) {
  SCOPED_MTX_LOCK(&info_lock) return info.ping_interval;
}

int get_peer(void) {
  SCOPED_MTX_LOCK(&info_lock) return info.peer;
}

int get_first_successor(int wait) {
  SCOPED_MTX_LOCK(&info_lock) {
    while (wait && info.first_successor == -1) {
      printf("I'm in the middle of getting my next first successor so I'll wait...\n");
      pthread_cond_wait(&info_wait, &info_lock);
    }

    return info.first_successor;
  }
}

int get_second_successor(int wait) {
  SCOPED_MTX_LOCK(&info_lock) {
    while (wait && info.second_successor == -1) {
      printf("I'm in the middle of getting my next second successor so I'll wait...\n");
      pthread_cond_wait(&info_wait, &info_lock);
    }

    return info.second_successor;
  }
}

int clear_first_successor(void) {
  SCOPED_MTX_LOCK(&info_lock) {
    drop_ping_info(info.first_successor + MIN_PEER_PORT);
    int tmp = info.first_successor;
    info.first_successor = -1;
    return tmp;
  }
}

int clear_second_successor(void) {
  SCOPED_MTX_LOCK(&info_lock) {
    drop_ping_info(info.second_successor + MIN_PEER_PORT);
    int tmp = info.second_successor;
    info.second_successor = -1;
    return tmp;
  }
}

int set_first_successor(int next) {
  SCOPED_MTX_LOCK(&info_lock) {
    if (info.first_successor != -1) {
      return info.first_successor;
    } else {
      initialise_ping_info(IP_ADDR, next);
      info.first_successor = next;
    }
  }

  pthread_cond_broadcast(&info_wait);
  return 0;
}

int set_second_successor(int next) {
  SCOPED_MTX_LOCK(&info_lock) {
    if (info.second_successor != -1) {
      return info.second_successor;
    } else {
      initialise_ping_info(IP_ADDR, next);
      info.second_successor = next;
    }
  }

  pthread_cond_broadcast(&info_wait);
  return 0;
}

void clear_and_set_successors(int first, int second) {
  SCOPED_MTX_LOCK(&info_lock) {
    if (info.first_successor != first) {
      drop_ping_info(info.first_successor + MIN_PEER_PORT);
      initialise_ping_info(IP_ADDR, first);
    }

    if (info.second_successor != second) {
      drop_ping_info(info.second_successor + MIN_PEER_PORT);
      initialise_ping_info(IP_ADDR, second);
    }

    info.first_successor = first;
    info.second_successor = second;
  }

  pthread_cond_broadcast(&info_wait);
}

void close_peer(void) {
  destroy_ping_module();
}

void verify_peers() {
  // TODO: Errors
  int first = get_first_successor(0);
  int second = get_second_successor(0);
  sleep(1);
  if (first != -1) {
    first = initialise_ping_info(IP_ADDR, first);
    send_pingfd(first, PING_REQ, 0);
  }

  if (second != -1) {
    second = initialise_ping_info(IP_ADDR, second);
    send_pingfd(second, PING_REQ, 0);
  }
}

pthread_t setup_ping_interval() {
  pthread_t thrd;
  pthread_create(&thrd, NULL, ping_thrd_ticker, NULL);
  return thrd;
}