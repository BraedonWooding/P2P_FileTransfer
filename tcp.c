#include "tcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "p2p_peer.h"
#include "ping.h"
#include "utils.h"

#define BUF_LEN (2048)

#define TCP_MSG(x) (#x)

static void *client_accept(void *client_id);
static int tcp_perform_send(int socket, int peer, char buf[]);

void *tcp_watcher(void *_) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_addr = {.s_addr = INADDR_ANY},
      .sin_port = htons(get_peer() + MIN_PEER_PORT),
  };

  bind(sock, (struct sockaddr *)&addr, sizeof(addr));
  listen(sock, MAX_PENDING);

  socklen_t addr_bytes = sizeof(addr);
  for (;;) {
    int client_fd = accept(sock, NULL, NULL);
    pthread_t thrd;
    pthread_create(&thrd, NULL, client_accept, (void *)(size_t)client_fd);
  }

  pthread_exit(NULL);
}

int tcp_send_join_req(int known_peer, int self) {
  char buf[BUF_LEN];
  snprintf(buf, BUF_LEN, "%s %d", TCP_MSG(TCP_JOIN_REQ), self);
  return tcp_send_new_socket(known_peer, buf);
}

int tcp_send_abrupt(int known, int left) {
  char buf[BUF_LEN];
  int send_socket = socket(AF_INET, SOCK_STREAM, 0);

  snprintf(buf, BUF_LEN, "%s %d", TCP_MSG(TCP_SUCC), left);
  tcp_perform_send(send_socket, known, buf);

  int bytes = recv(send_socket, buf, BUF_LEN, 0);
  if (bytes == 0) return -1;
  buf[bytes] = '\0';

  READ_MSG_TYPE(0, buf, " ");
  int first = READ_MSG_POSINT(0);

  shutdown(send_socket, SHUT_RD);
  close(send_socket);

  return first;
}

void tcp_send_quit_req(void) {
  char buf[BUF_LEN];
  int preds[MAX_PING_FDS];
  int count = get_preds(preds);

  for (int i = 0; i < count; i++) {
    printf("> Sending exit msg to %d\n", preds[i]);
    snprintf(buf, BUF_LEN, "%s %d %d %d", TCP_MSG(TCP_PEER_DEPART), get_peer(),
             get_first_successor(0), get_second_successor(0));
    tcp_send_new_socket(preds[i], buf);
  }
}

int tcp_send_new_socket(int peer, char buf[]) {
  int send_socket = socket(AF_INET, SOCK_STREAM, 0);

  int sent = tcp_perform_send(send_socket, peer, buf);

  shutdown(send_socket, SHUT_RD);
  close(send_socket);

  return sent;
}

static int tcp_perform_send(int socket, int peer, char buf[]) {
  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_addr = {.s_addr = inet_addr(IP_ADDR)},
      .sin_port = htons(peer + MIN_PEER_PORT),
  };

  connect(socket, (struct sockaddr *)&addr, sizeof(addr));
  ssize_t count = 0;
  count = send(socket, buf, strlen(buf), 0);
  return count >= 0 ? count : -1;
}

void *client_accept(void *arg) {
  int client_fd = (size_t)arg;
  char buf[BUF_LEN];

  for (;;) {
    int bytes = recv(client_fd, buf, BUF_LEN, 0);
    if (bytes == 0) break;
    buf[bytes] = '\0';

    READ_MSG_TYPE(0, buf, " ");

    if (get_first_successor(0) == -1 || get_second_successor(0) == -1) {
      // we haven't loaded our successors yet...
      if (!strcasecmp(buf, TCP_MSG(TCP_JOIN_RESP))) {
        int first = READ_MSG_POSINT(0);
        int second = READ_MSG_POSINT(0);
        clear_and_set_successors(first, second);
        break;
      } else {
        fprintf(stderr,
                "Error: Unknown type %s closing connection "
                "(I'm awaiting initialisation)\n",
                buf);
        break;
      }
    } else if (!strcasecmp(buf, TCP_MSG(TCP_JOIN_REQ))) {
      // peer wishing to join
      int peer = READ_MSG_POSINT(0);
      int first_succ = get_first_successor(1);
      int second_succ = get_second_successor(1);
      int third_succ = -1;

      if (peer > first_succ) {
        // pass it on...
        printf("> Peer %d Join request forwarded to successor\n", first_succ);
        tcp_send_join_req(first_succ, peer);

        if (peer < second_succ) {
          // they are going to become our new second_succ
          printf("> My first successor remains unchanged at Peer %d\n",
                 first_succ);
          printf("> My new second successor is Peer %d\n", peer);
          clear_and_set_successors(first_succ, peer);
        }
      } else {
        printf("> Peer %d join request received\n", peer);
        printf("> My new first successor is %d\n", peer);
        printf("> My new second successor is %d\n", first_succ);
        clear_and_set_successors(peer, first_succ);
        // we are also going to then send a successor update
        // to the peer informing them of their successors
        sprintf(buf, "%s %d %d", TCP_MSG(TCP_JOIN_RESP), first_succ,
                second_succ);
        tcp_send_new_socket(peer, buf);
      }
    } else if (!strcasecmp(buf, TCP_MSG(TCP_PEER_DEPART))) {
      // peer departing
      int peer = READ_MSG_POSINT(0);
      // swap the peer departing with one of these peers
      int first = READ_MSG_POSINT(0);
      int second = READ_MSG_POSINT(0);
      int min, max;
      if (first > second) max = first, min = second;
      else                min = second, max = first;
      printf("> Peer %d will depart from the network\n", peer);
      first = get_first_successor(1);

      if (peer == first) {
        clear_and_set_successors(min, max);
        printf("> My new first successor is %d\n", min);
        printf("> My new second successor is %d\n", max);
      } else if (peer == get_second_successor(1)) {
        clear_and_set_successors(first, min);
        printf("> My new first successor is %d\n", first);
        printf("> My new second successor is %d\n", min);
      } else {
        printf("> I have no relation to this peer so I'll ignore\n");
      }
    } else if (!strcasecmp(buf, TCP_MSG(TCP_SUCC))) {
      // used for abrupt depart
      // peer wanting request
      int peer = READ_MSG_POSINT(0);
      // peer that was detected to have left
      int left = READ_MSG_POSINT(0);
      // wait for our successors to be valid
      // (we want both successors to be valid... but we only care
      // about using the first successor)
      (void)get_second_successor(1);
      int first = get_first_successor(1);
      printf("> Peer %d left abruptly sending %d to %d as new peer\n", left,
             first, peer);

      snprintf(buf, BUF_LEN, "%s %d", TCP_MSG(TCP_SUCC), first);
      // send back the information
      tcp_perform_send(client_fd, peer, buf);
    } else if (!strcasecmp(buf, TCP_MSG(TCP_STORE))) {
      // the hash of the filename
      int expected_peer = READ_MSG_POSINT(0);
      // the actual file id
      int file_id = READ_MSG_POSINT(0);
    } else if (!strcasecmp(buf, TCP_MSG(TCP_RETRIEVE))) {
      // the hash of the filename
      int expected_peer = READ_MSG_POSINT(0);
      // the actual file id
      int file_id = READ_MSG_POSINT(0);
    } else {
      fprintf(stderr, "Error: Unknown type %s closing connection\n", buf);
      break;
    }
  }

  shutdown(client_fd, SHUT_RD);
  close(client_fd);
  return NULL;
}
