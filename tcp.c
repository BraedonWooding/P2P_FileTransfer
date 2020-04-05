#include "tcp.h"

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
        pthread_create(&thrd, NULL, client_accept, (void*)(size_t)client_fd);
    }

    pthread_exit(NULL);
}

int tcp_send_join_req(int known_peer, int self) {
    char buf[BUF_LEN];
    sprintf(buf, "%s %d", TCP_MSG(TCP_JOIN_REQ), self);
    return tcp_send_new_socket(known_peer, buf);
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
    ssize_t bytes_served = 0;
    while (count > 0 || !bytes_served) {
        count = send(socket, buf + bytes_served, strlen(buf) - bytes_served, 0);
        if (count >= 0) bytes_served += count;
    }

    return count >= 0 ? count : -1;
}

void *client_accept(void *arg) {
    int client_fd = (size_t)arg;
    char buf[BUF_LEN];

    for (;;) {
        int bytes = recv(client_fd, buf, BUF_LEN, 0);
        if (bytes == 0) break;

        READ_MSG_TYPE(0, buf, " ");

        if (get_first_successor() == -1) {
            // we haven't loaded our successors yet...
            if (!strcasecmp(buf, TCP_MSG(TCP_JOIN_RESP))) {
                int first = READ_MSG_POSINT(0);
                int second = READ_MSG_POSINT(0);
                clear_and_set_successors(first, second);
                break;
            } else {
                fprintf(stderr, "Error: Unknown type %s closing connection "
                                "(I'm awaiting initialisation)\n", buf);
                break;
            }
        } else if (!strcasecmp(buf, TCP_MSG(TCP_JOIN_REQ))) {
            // peer wishing to join
            int peer = READ_MSG_POSINT(0);
            int first_succ = get_first_successor();
            int second_succ = get_second_successor();
            int third_succ = -1;

            if (peer > first_succ) {
                // pass it on...
                printf("> Peer %d Join request forwarded to successor\n", peer);
                tcp_send_join_req(first_succ, peer);

                if (peer < second_succ) {
                    // they are going to become our new second_succ
                    printf("> My first successor remains unchanged at Peer %d\n",
                            first_succ);
                    printf("> My new second successor is Peer %d\n", peer);
                    clear_and_set_successors(first_succ, peer);
                }
            } else {
                clear_and_set_successors(peer, first_succ);
                // we are also going to then send a successor update
                // to the peer informing them of their successors
                sprintf(buf, "%s %d %d", TCP_MSG(TCP_JOIN_RESP),
                        first_succ, second_succ);
                tcp_send_new_socket(peer, buf);
            }
        } else if (!strcasecmp(buf, TCP_MSG(TCP_PEER_DEPART))) {
            // peer departing
            int peer = READ_MSG_POSINT(0);
            // swap the peer departing with this peer
            int swp = READ_MSG_POSINT(0);
        } else if (!strcasecmp(buf, TCP_MSG(TCP_SUCC))) {
            // used for abrupt depart
            // peer wanting request
            int peer = READ_MSG_POSINT(0);
            // peer that was detected to have left
            int left = READ_MSG_POSINT(0);
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
