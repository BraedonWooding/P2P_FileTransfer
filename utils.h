/*
  Author: Braedon Wooding (z5204996)
 */

#ifndef __P2P_UTILS_H__
#define __P2P_UTILS_H__

/**                                                     **
 * A collection of useful macros to perform common tasks *
 **                                                     **/

#define MTX_LOCK_BLOCK(mtx, block) { \
  mtx_lock((mtx)); \
  { block } \
  mtx_unlock((mtx)); \
}

int try_parse_strtol(char *prog, char *in, long int *out);

#endif