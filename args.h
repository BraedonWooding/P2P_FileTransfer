/*
  Author: Braedon Wooding (z5204996)
 */

#ifndef __P2P_ARGS_H__
#define __P2P_ARGS_H__

/**                                               **
 * A simple arg parser for command line arguments  *
 * General purpose that I used for a lot of things *
 **                                               **/

#define INIT_ARGS(argc, argv) \
  int arg_parser_cur = 0; \
  int arg_parser_argc = argc; \
  char **arg_parser_argv = argv

// Protection against common execv mistakes
#define SKIP_PROGNAME() do { \
  if (arg_parser_cur == 0 && arg_parser_argc > 0) arg_parser_cur++; \
  else { fprintf(stderr, "Missing Progam Name\n"); exit(1); }\
} while(0)

#define READ_STR() \
  (arg_parser_cur < argc ? arg_parser_argv[arg_parser_cur++] : NULL)
#define READ_INT(into) do { \
  if (arg_parser_cur >= arg_parser_argc) { \
    fprintf(stderr, "Error [%s]: %s is missing!\n", arg_parser_argv[0], #into);\
    USAGE_EXIT(); \
  } else if (!try_parse_strtol(arg_parser_argv[0], \
                        arg_parser_argv[arg_parser_cur++], (into))) \
    USAGE_EXIT(); \
} while(0)

// custom err msg
#define USAGE_EXIT() do { \
  fprintf(stderr, \
"Usage %s init <peer: int> <successor 1: int> <successor 2: int> <ping: int>\n"\
"      %s join <peer: int> <known peer: int> <ping: int>\n", \
          arg_parser_argv[0], arg_parser_argv[0]); \
  exit(1); } while(0)

#endif