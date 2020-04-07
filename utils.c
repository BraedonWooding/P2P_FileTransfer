#include "utils.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void set_sockaddr(struct sockaddr_in *sock, char *ip, int port) {
  memset(sock, 0, sizeof(*sock));
  sock->sin_family = AF_INET; // IPv4;
  sock->sin_port = htons(port); // ensure big endian
  inet_pton(AF_INET, ip, &sock->sin_addr);
}

/*
  Try to parse a positive integer.
  If parsing fails or if it parses a negative integer
  it'll return -1.
 */
int try_parse_posint(char *in) {
  int out;
  if (!try_parse_strtol("", in, &out)) {
    return -1;
  } else if (out < 0) {
    fprintf(stderr, "Error %d is negative.\n", out);
    return -1;
  } else {
    return out;
  }
}

int try_parse_strtol(char *prog, char *in, int *out) {
  if (in == NULL) {
    return 0;
  }

  char *end = NULL;
  errno = 0;
  long int tmp = strtol(in, &end, 10);
  if (tmp > (long)INT_MAX) *out = INT_MAX;
  if (tmp < (long)INT_MIN) *out = INT_MIN;
  *out = tmp;

  if (in == end) {
    fprintf(stderr, "Error %s: %s is not a number.\n", prog, in);
  } else if (errno == ERANGE) {
    char *msg = tmp == LONG_MAX ? "overflowed" : "underflowed";
    fprintf(stderr, "Error %s: %s %s.\n", prog, in, msg);
  } else if (errno && !*out) {
    fprintf(stderr, "Error %s: Unknown error for %s.\n", prog, in);
  } else if (!errno && *end) {
    fprintf(stderr, "Error %s: %s extra characters detected.\n", prog, in);
    int before = strlen(in) - strlen(end) + strlen(prog) - 2;
    fprintf(stderr, "From here: %*s^\n", before, "");
  } else {
    return 1;
  }

  return 0;
}
