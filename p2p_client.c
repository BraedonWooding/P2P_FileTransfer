#include "p2p_client.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "args.h"
#include "utils.h"

int main(int argc, char *argv[]) {
    INIT_ARGS(argc, argv);
    SKIP_PROGNAME();

    char *subcommand = READ_STR();
    if (!subcommand) {
        fprintf(stderr, "Error [%s]: missing subcommand!\n", argv[0]);
        USAGE_EXIT();
    }

    if (!strcmp(subcommand, "init")) {
        long int peer, first_succesor, second_successor, ping;
        READ_INT(&peer);
        READ_INT(&first_succesor);
        READ_INT(&second_successor);
        READ_INT(&ping);
    } else if (!strcmp(subcommand, "join")) {
        long int peer, known_peer, ping;
        READ_INT(&peer);
        READ_INT(&known_peer);
        READ_INT(&ping);
    } else {
        fprintf(stderr, "Error [%s]: %s is not a valid subcommand\n",
                argv[0], subcommand);
        USAGE_EXIT();
    }

    return 0;
}
