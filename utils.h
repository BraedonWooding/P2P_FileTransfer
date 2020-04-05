/*
  Author: Braedon Wooding (z5204996)
 */

#ifndef __P2P_UTILS_H__
#define __P2P_UTILS_H__

#include <pthread.h>
#include <arpa/inet.h>

/**                                                     **
 * A collection of useful macros to perform common tasks *
 **                                                     **/

/*
  == Performance of the lock macros ==

  Through some small investigations (godbolt + others)
  It's very clear that running this on any kind of optimisation (>= O1)
  and with always inlining the cleanup call it can eliminate the need
  for the temporary struct (completely) and becomes practically identical
  to if you had inserted the calls at each return callsite.
*/

#define _CONCAT(t1, t2) t1 ## t2
#define CONCAT(t1, t2) _CONCAT(t1, t2)

#define CLEANUP_ATTR(fn) __attribute__((cleanup(fn)))
typedef void (*cleanup_handle_fn)(void *arg);

#define UNUSED_ATTR __attribute__((unused))
#define INLINE_ATTR __attribute__((always_inline))

typedef struct cleanup_callback_t {
  void *arg;
  cleanup_handle_fn cleanup_handle;
} cleanup_callback;

inline void cleanup_call(cleanup_callback *cleanup) INLINE_ATTR;

#define SCOPED_REGION(acq, rel, _arg) \
  if (0) { \
    CONCAT(_done_, __LINE__): \
    ; \
  } else for (cleanup_callback _locked_scope_tmp CLEANUP_ATTR(cleanup_call) \
    = (cleanup_callback){ .arg = _arg, .cleanup_handle = rel }; acq(_arg), 1;) \
    if (1) \
      goto CONCAT(_body_, __LINE__); \
    else \
      while (1) \
        if (1) \
          goto CONCAT(_done_, __LINE__); \
        else \
          CONCAT(_body_, __LINE__):

#define SCOPED_MTX_LOCK(lock) \
  SCOPED_REGION(pthread_mutex_lock,(cleanup_handle_fn)pthread_mutex_unlock,lock)

void set_sockaddr(struct sockaddr_in *sock, char *ip, int port);

int try_parse_strtol(char *prog, char *in, int *out);
int try_parse_posint(char *in);

/*
  Useful only for the kind of TCP / UDP msgs I used
  where I always put a string type at the front

  NOTE: Exposes a ridiculous number of variables to scope
        that is why you have to provide an id to prevent them to clash
        if you want the recurrent properties.
        (Benefits of this is it makes future calls nicer :D)
 */
#define READ_MSG_TYPE(id, buf, delim) \
  char *_save_ptr_##id; \
  char *_delim_##id = delim; \
  char *_tok_##id; \
  do { \
    _tok_##id = strtok_r(buf, _delim_##id, &_save_ptr_##id); \
    if (!_tok_##id) { \
      fprintf(stderr, "Error: Missing msg type\n"); \
      buf[0] = '\0'; \
    } \
  } while (0)

#define READ_MSG_POSINT(id) \
  (_tok_##id = strtok_r(NULL, _delim_##id, &_save_ptr_##id), \
  try_parse_posint(_tok_##id))

#endif