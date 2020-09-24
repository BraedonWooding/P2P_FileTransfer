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

#define PEER_HASH(x) ((x % 256))

#define _CONCAT(t1, t2) t1 ## t2
#define CONCAT(t1, t2) _CONCAT(t1, t2)

#define CLEANUP_ATTR(fn) __attribute__((cleanup(fn)))
typedef void (*cleanup_handle_fn)(void *arg);

#define UNUSED_ATTR __attribute__((unused))
#define INLINE_ATTR __attribute__((always_inline))

/*
  Abstracted form of a cleanup.  You shouldn't not need to use this in
  any way.  It is wrapped entirely by the guard macros.
*/
typedef struct cleanup_callback_t {
  void *arg;
  cleanup_handle_fn cleanup_handle;
} cleanup_callback;

// Force inline of cleanup call!!
// extremely important for optimisation
inline void cleanup_call(cleanup_callback *cleanup) INLINE_ATTR;

inline void cleanup_call(cleanup_callback *cleanup) {
  // as you can see it's a very straightforward cleanup.
  // we don't call your arg with NULL though because we expect
  // you to handle NULL as a sentinel.
  if (cleanup->arg) cleanup->cleanup_handle(cleanup->arg);
}

/*
  Define a scoped region using some goto magic

  Roughly this effectively is just doing
  {
    acq()
    {
      // your block
    }
    rel()
  }

  We have to define it like this to prevent accidental
  leaks i.e. if you do
  MTX_LOCK(&lock) if (a) printf("Something")

  The lock should only exist inside that if statement
  we use the amazing cleanup attribute to do the majority
  of the work for us and the rest is just us being clever
  with for loops to create the variables with a given scope
  and then using gotos to simulate a block control flow.

  As said before majority of this is easily compiled away
  since everything gets inlined very nicely.
  and the compiler can very easily see that no for loop
  is actually run more than once.
*/
#define SCOPED_REGION(init, var) \
  if (0) { \
    CONCAT(_done_, __LINE__): \
    ; \
  } else for (init) for (var ;;) \
    if (1) \
      goto CONCAT(_body_, __LINE__); \
    else \
      while (1) while(1) \
        if (1) \
          goto CONCAT(_done_, __LINE__); \
        else \
          CONCAT(_body_, __LINE__):

/*
  Useful if you have a lock styled piece of code.
  will bind as much as a forloop binds.
*/
#define SCOPED_LOCK(acq, rel, lock) \
  SCOPED_REGION(cleanup_callback _locked_scope_tmp CLEANUP_ATTR(cleanup_call) \
    = ((cleanup_callback){ .arg = lock, .cleanup_handle = (cleanup_handle_fn)rel }); \
    (acq(lock), 1);,)

/*
  Pythonic styled with.
  Binds as much as a forloop binds.
*/
#define SCOPED_WITH(type, name, init, rel, ...) \
  SCOPED_REGION(cleanup_callback _locked_scope_tmp CLEANUP_ATTR(cleanup_call) \
    = ((cleanup_callback){ .arg = init(__VA_ARGS__), .cleanup_handle = (cleanup_handle_fn)rel });;, \
    type name = (type)_locked_scope_tmp.arg)

#define SCOPED_MTX_LOCK(lock) \
  SCOPED_LOCK(pthread_mutex_lock, pthread_mutex_unlock, lock)

#define SCOPED_FILE(name, path, mode) \
  SCOPED_WITH(FILE *, name, fopen, fclose, path, mode)

void set_sockaddr(struct sockaddr_in *sock, char *ip, int port);

/*
  Try to parse a long integer.  Just a loose wrapper around strtol
*/
int try_parse_strtol(char *prog, char *in, int *out);

/*
  Try to parse a positive 32 bit integer.
  If parsing fails or if it parses a negative integer or if the integer
  is too large it'll return -1.
 */
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
  do { \
    if (!strtok_r(buf, _delim_##id, &_save_ptr_##id)) { \
      fprintf(stderr, "Error: Missing msg type\n"); \
      buf[0] = '\0'; \
    } \
  } while (0)

#define READ_MSG_POSINT(id) \
  try_parse_posint(strtok_r(NULL, _delim_##id, &_save_ptr_##id))

#define READ_MSG_STR(id) \
  strtok_r(NULL, _delim_##id, &_save_ptr_##id)

#endif