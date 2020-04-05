#include "entry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"
#include "utils.h"
#include "p2p_peer.h"

#define BUF_LEN (1024)

int main(int argc, char *argv[]) {
  INIT_ARGS(argc, argv);
  SKIP_PROGNAME();

  char *subcommand = READ_STR();
  if (!subcommand) {
    fprintf(stderr, "Error [%s]: missing subcommand!\n", argv[0]);
    USAGE_EXIT();
  }

  pthread_t ping_ticker, ping_rec;
  if (!strcasecmp(subcommand, "init")) {
    int peer, first_succesor, second_successor, ping;
    READ_INT(&peer);
    READ_INT(&first_succesor);
    READ_INT(&second_successor);
    READ_INT(&ping);
    ping_rec = init_peer(peer, first_succesor, second_successor, ping);
  } else if (!strcasecmp(subcommand, "join")) {
    int peer, known_peer, ping;
    READ_INT(&peer);
    READ_INT(&known_peer);
    READ_INT(&ping);
    ping_rec = join_peer(peer, known_peer, ping);
  } else {
    fprintf(stderr, "Error [%s]: %s is not a valid subcommand\n", argv[0],
            subcommand);
    USAGE_EXIT();
  }

  verify_peers();

  ping_ticker = setup_ping_interval();

  char read_buf[BUF_LEN];
  while (fgets(read_buf, BUF_LEN, stdin)) {
    READ_MSG_TYPE(0, read_buf, " ");
    if (!strcasecmp(read_buf, "store")) {
      int file = READ_MSG_POSINT(0);
      // TODO:
      fprintf(stderr, "TODO");
    } else if (!strcasecmp(read_buf, "request")) {
      int file = READ_MSG_POSINT(0);
      // TODO:
      fprintf(stderr, "TODO");
    } else if (!strcasecmp(read_buf, "quit")) {
      // TODO: Send quit msgs
      //       once they are done
      //       we can cancel threads
      //       and then leave...
      break;
    }
  }

  pthread_cancel(ping_ticker);
  pthread_cancel(ping_rec);

  printf("Peer %d closing down", get_peer());
  close_peer();

  return 0;
}
