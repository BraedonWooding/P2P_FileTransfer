#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

int try_parse_strtol(char *prog, char *in, long int *out) {
    char *end = NULL;
    errno = 0;
    *out = strtol(in, &end, 10);
    if (in == end) {
        fprintf(stderr, "Error [%s]: %s is not a number.\n", prog, in);
    } else if (errno == ERANGE) {
        char *msg = *out == LONG_MAX ? "overflowed" : "underflowed";
        fprintf(stderr, "Error [%s]: %s %s.\n", prog, in, msg);
    } else if (errno && !*out) {
        fprintf(stderr, "Error [%s]: Unknown error for %s.\n", prog, in);
    } else if (!errno && *end) {
        fprintf(stderr, "Error [%s]: %s extra characters detected.\n", prog, in);
        int before = strlen(in) - strlen(end) + strlen(prog) -2;
        fprintf(stderr, "From here: %*s^\n", before, "");
    } else {
        return 1;
    }
    return 0;
}
