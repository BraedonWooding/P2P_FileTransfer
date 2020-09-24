#include "tcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <strings.h>
#include <unistd.h>

#include "p2p_peer.h"
#include "ping.h"
#include "utils.h"

#define BUF_LEN (2048)

#define TCP_MSG(x) (#x)

typedef struct file_node_t {
  struct file_node_t *next;
  int fileId;
} file_node;

static file_node *head = NULL;
static pthread_mutex_t head_lock = PTHREAD_MUTEX_INITIALIZER;

static void *client_accept(void *client_id);
static int tcp_perform_send(int socket, int peer, char buf[]);

void cleanup_handler(void *arg) { 
  int sock = (size_t)arg;
  shutdown(sock, SHUT_RDWR);
  close(sock);
}

void *tcp_watcher(void *_) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  int on = 1;

  pthread_cleanup_push(cleanup_handler, (void*)(size_t)sock);
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) {
    perror("setsockopt");
  }
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int))) {
    perror("setsockopt");
  }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_addr = {.s_addr = INADDR_ANY},
    .sin_port = htons(get_peer() + MIN_PEER_PORT),
  };

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr))) {
    printf("> TCP Bind failed :(");
    perror("bind");
  }

  listen(sock, MAX_PENDING);

  socklen_t addr_bytes = sizeof(addr);
  for (;;) {
    int client_fd = accept(sock, NULL, NULL);
    pthread_t thrd;
    pthread_create(&thrd, NULL, client_accept, (void *)(size_t)client_fd);
  }

  pthread_cleanup_pop(1);

  pthread_exit(NULL);
}

int tcp_send_store_req(int file, int peer_requesting, int peer) {
  char buf[BUF_LEN];
  snprintf(buf, BUF_LEN, "%s %d %d", TCP_MSG(TCP_STORE), file, peer_requesting);
  return tcp_send_new_socket(peer, buf);
}

int tcp_send_retrieve_req(int file, int peer_requesting, int peer) {
  char buf[BUF_LEN];
  snprintf(buf, BUF_LEN, "%s %d %d", TCP_MSG(TCP_RETRIEVE), file, peer_requesting);
  return tcp_send_new_socket(peer, buf);
}

void tcp_transfer_send(int file, char *ext, int peer) {
  char buf[BUF_LEN];
  int send_socket = socket(AF_INET, SOCK_STREAM, 0);

  snprintf(buf, BUF_LEN, "%d.%s", file, ext);

  SCOPED_FILE(f, buf, "r") {
    if (!f) return;

    printf("> Sending %s\n", buf);
    snprintf(buf, BUF_LEN, "%s %d %d.%s ", TCP_MSG(TCP_TRANSFER), file, file, ext);
    tcp_perform_send(send_socket, peer, buf);

    while (fgets(buf, BUF_LEN, f) != NULL) {
      tcp_perform_send(send_socket, peer, buf);
    }

    shutdown(send_socket, SHUT_RD);
    close(send_socket);
  }
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
  if (tcp_perform_send(send_socket, known, buf) < 0) {
    shutdown(send_socket, SHUT_RD);
    close(send_socket);
    return -1;
  }

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

  if (connect(socket, (struct sockaddr *)&addr, sizeof(addr))) return -1;
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
      int file_id = READ_MSG_POSINT(0);
      int peer = READ_MSG_POSINT(0);
      int hash = PEER_HASH(file_id);
      int first_succ = get_first_successor(1);
      int second_succ = get_second_successor(1);

      // if we are looping we want to store, or if the hash is <
      // or if the hash is a good match.
      if (hash == get_peer() || hash < get_peer() ||
          first_succ < get_peer()) {
        printf("> Store %d request accepted\n", file_id);
        SCOPED_MTX_LOCK(&head_lock) {
          file_node *new_head = malloc(sizeof(*new_head));
          new_head->next = head;
          new_head->fileId = file_id;
          head = new_head;
        }
      } else {
        // pass it on...
        printf("> Store %d request forwarded to successor\n", file_id);
        tcp_send_store_req(file_id, peer, first_succ);
      }
    } else if (!strcasecmp(buf, TCP_MSG(TCP_RETRIEVE))) {
      int file_id = READ_MSG_POSINT(0);
      int peer = READ_MSG_POSINT(0);
      int hash = PEER_HASH(file_id);
      int first_succ = get_first_successor(1);
      int second_succ = get_second_successor(1);

      // check if file is in peer
      file_node *cur;
      SCOPED_MTX_LOCK(&head_lock) for (cur = head; cur; cur = cur->next) {
        if (cur->fileId == file_id) {
          printf("> Retrieve %d request accepted\n", file_id);
          tcp_transfer_send(file_id, "txt", peer);
          tcp_transfer_send(file_id, "pdf", peer);
          break;
        }
      }

      if (!cur) {
        if (peer == get_peer()) {
          printf("> Couldn't find file! %d\n", file_id);
        } else {
          printf("> Retrieve %d request forwarded to successor\n", file_id);
          tcp_send_retrieve_req(file_id, peer, first_succ);
        }
      }
    } else if (!strcasecmp(buf, TCP_MSG(TCP_TRANSFER))) {
      int file = READ_MSG_POSINT(0);
      // they sending file to us
      // read filename
      char *filename = READ_MSG_STR(0);
      char *cur = filename + strlen(filename) + 1;
      // bit of weird shit here so we can finish off our buffer
      // before we try another read.
      size_t bytes_left = cur - buf;
      char filebuf[BUF_LEN];
      snprintf(filebuf, BUF_LEN, "received_%s", filename);
      SCOPED_FILE(f, filebuf, "w") for (;;) {
        while (bytes_left < bytes) {
          bytes_left += fwrite(cur, 1, bytes - bytes_left, f);
        }

        bytes = recv(client_fd, buf, BUF_LEN, 0);
        if (bytes == 0) break;
        buf[bytes] = '\0';
        cur = buf;
        bytes_left = 0;
      }
      printf("> Receieved %s\n", filebuf);
    } else {
      fprintf(stderr, "Error: Unknown type %s closing connection\n", buf);
      break;
    }
  }

  shutdown(client_fd, SHUT_RD);
  close(client_fd);
  return NULL;
}
