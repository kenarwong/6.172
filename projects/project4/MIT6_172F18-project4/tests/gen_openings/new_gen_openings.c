// Copyright (c) 2015 MIT License by 6.172 Staff

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <pthread.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <cilk/cilk.h>
#include <cilk/reducer.h>

#include "../../player/eval.h"
#include "../../player/fen.h"
#include "../../player/move_gen.h"
#include "../../player/search.h"
#include "../../player/tt.h"
#include "../../player/util.h"


char  VERSION[] = "1038";


#define MAX_HASH 4096       // 4 GB
#define INF_TIME 99999999999.0
#define INF_DEPTH 999       // if user does not specify a depth, use 999

// if the time remain is less than this fraction, dont start the next search iteration
#define RATIO_FOR_TIMEOUT 0.5

// -----------------------------------------------------------------------------
// file I/O
// -----------------------------------------------------------------------------

static FILE* OUT;

// Options for UCI interface

// defined in search.c
extern int DRAW;
extern int LMR_R1;
extern int LMR_R2;
extern int HMB;
extern int USE_NMM;
extern int FUT_DEPTH;
extern int TRACE_MOVES;
extern int DETECT_DRAWS;

// defined in eval.c
extern int RANDOMIZE;
extern int HATTACK;
extern int PBETWEEN;
extern int KFACE;
extern int KAGGRESSIVE;
extern int MOBILITY;

// defined in move_gen.c
extern int USE_KO;

// defined in tt.c
extern int USE_TT;
extern int HASH;

typedef struct {
  char      name[MAX_CHARS_IN_TOKEN];   // name of options
  int*       var;       // pointer to an int variable holding its value
  int       dfault;     // default value
  int       min;        // lower bound on what we want it to be
  int       max;        // upper bound
} int_options;

// Configurable options for passing via UCI interface.
// This logic could be expanded to support the various UCI option
// types more faithfully.  Right now, everything is pretty much a
// "spin" type.

static int_options iopts[] = {
  // name                      variable    default                lower bound     upper bound
  // ---------------------------------------------------------------------------------------------
  { "hattack",                 &HATTACK,   0.02 * PAWN_EV_VALUE,  0,              PAWN_EV_VALUE },
  { "mobility",               &MOBILITY,   0.02 * PAWN_EV_VALUE,  0,              PAWN_EV_VALUE },
  { "kaggressive",         &KAGGRESSIVE,   1.0 * PAWN_EV_VALUE,   0,              PAWN_EV_VALUE },
  { "kface",                     &KFACE,   0.3 * PAWN_EV_VALUE,   0,              PAWN_EV_VALUE },
  { "pbetween",               &PBETWEEN,   0.3 * PAWN_EV_VALUE,   -PAWN_EV_VALUE, PAWN_EV_VALUE },
  { "hash",                       &HASH,   16,                    1,              MAX_HASH   },
  { "draw",                       &DRAW,   -0.07 * PAWN_VALUE,    -PAWN_VALUE,    PAWN_VALUE    },
  { "randomize",             &RANDOMIZE,   0,                     0,              PAWN_EV_VALUE },
  { "lmr_r1",                   &LMR_R1,   5,                     1,              MAX_NUM_MOVES },
  { "lmr_r2",                   &LMR_R2,   20,                    1,              MAX_NUM_MOVES },
  { "hmb",                         &HMB,   0.03 * PAWN_VALUE,     0,              PAWN_VALUE    },
  { "fut_depth",             &FUT_DEPTH,   3,                     0,              5             },
  // debug options
  { "use_nmm",                 &USE_NMM,   1,                     0,              1             },
  { "detect_draws",       &DETECT_DRAWS,   1,                     0,              1             },
  { "use_tt",                   &USE_TT,   1,                     0,              1             },
  { "use_ko",                   &USE_KO,   1,                     0,              1             },
  { "trace_moves",         &TRACE_MOVES,   0,                     0,              1             },
  { "",                            NULL,   0,                     0,              0             },
};

// -----------------------------------------------------------------------------
// Printing helpers
// -----------------------------------------------------------------------------

void getPV(move_t* pv, char* buf);

int file_exists(const char* filename) {
  struct stat sbuf;
  return stat(filename, &sbuf) == 0;
}

void lower_case(char* s) {
  int i;
  int c = strlen(s);

  for (i = 0; i < c; i++) {
    s[i] = tolower(s[i]);
  }

  return;
}

// Returns victims or NO_VICTIMS if no victims or -1 if illegal move
// makes the move described by 'mvstring'
victims_t make_from_string(position_t* old, position_t* p,
                           const char* mvstring) {
  sortable_move_t lst[MAX_NUM_MOVES];
  move_t mv = 0;
  // make copy so that mvstring can be a constant
  char string[MAX_CHARS_IN_MOVE];
  int move_count = generate_all(old, lst, true);

  snprintf(string, MAX_CHARS_IN_MOVE, "%s", mvstring);
  lower_case(string);

  for (int i = 0; i < move_count; i++) {
    char buf[MAX_CHARS_IN_MOVE];
    move_to_str(get_move(lst[i]), buf, MAX_CHARS_IN_MOVE);
    lower_case(buf);

    if (strcmp(buf, string) == 0) {
      mv = get_move(lst[i]);
      break;
    }
  }

  return (mv == 0) ? ILLEGAL() : make_move(old, p, mv);
}

typedef enum {
  NONWHITESPACE_STARTS,  // next nonwhitespace starts token
  WHITESPACE_ENDS,       // next whitespace ends token
  QUOTE_ENDS             // next double-quote ends token
} parse_state_t;


// -----------------------------------------------------------------------------
// UCI search (top level scout search call)
// -----------------------------------------------------------------------------

static move_t bestMoveSoFar;
static char theMove[MAX_CHARS_IN_MOVE];

static pthread_mutex_t entry_mutex;
static uint64_t node_count_serial;

typedef struct {
  position_t* p;
  int depth;
  double tme;
} entry_point_args;

void* entry_point(void* arg) {
  move_t subpv[MAX_PLY_IN_SEARCH];

  entry_point_args* real_arg = (entry_point_args*)arg;
  int depth = real_arg->depth;
  position_t* p = real_arg->p;
  double tme = real_arg->tme;

  double et = 0.0;

  // start time of search
  init_abort_timer(tme);

  init_best_move_history();
  tt_age_hashtable();

  init_tics();

  for (int d = 1; d <= depth; d++) {  // Iterative deepening
    reset_abort();

    searchRoot(p, -INF, INF, d, 0, subpv, &node_count_serial,
               OUT);

    et = elapsed_time();

#if PARALLEL
    // If we haven't aborted yet, store the best move we found.  Or, if we did
    // abort and haven't finished searching a single level, just take whatever
    // score we have for the principal variation node
    if (!is_aborted(&glob_abort) || d == 1) {
      bestMoveSoFar = subpv[0];
    } else {
      // we will use the bestMoveSoFar from the last level of iterative
      // deepening
      break;
    }
#else
    bestMoveSoFar = subpv[0];

    if (!should_abort()) {
      // print something?
    } else {
      break;
    }
#endif

    // don't start iteration that you cannot complete
    if (et > tme * RATIO_FOR_TIMEOUT) {
      break;
    }
  }

  // This unlock will allow the main thread lock/unlock in UCIBeginSearch to
  // proceed
  pthread_mutex_unlock(&entry_mutex);

  return NULL;
}

void UciBeginSearch(position_t* p, int depth, double tme) {
  pthread_mutex_lock(&entry_mutex);  // setup for the barrier

  entry_point_args args;
  args.depth = depth;
  args.p = p;
  args.tme = tme;

#if PARALLEL
  abort_constructor(&glob_abort, NULL);
  node_count_parallel = (Speculative_add)
                        CILK_C_INIT_REDUCER(Speculative_reducer,
                                            speculative_add_reduce,
                                            speculative_add_identity,
                                            speculative_add_destroy,
  (Speculative_reducer) {
    .value = 0, .last_value = 0,
     .abort_flag = false, .reset_flag = false,
      .deterministic = false, .real_total = 0
  });
  CILK_C_REGISTER_REDUCER(node_count_parallel);
#else
  node_count_serial = 0;
#endif

#if PARALLEL
  pthread_t play_thread;
  pthread_create(&play_thread, NULL, &entry_point, &args);

  // If we aren't searching to a fixed depth, terminate after a time threshold
  // has passed
  if (depth == INF_DEPTH) {
    usleep(tme * 100);
    do_abort(&glob_abort);
  }

  // these two lines implement a barrier (see mutex_unlock in UCIBeginSearch)
  pthread_mutex_lock(&entry_mutex);
  pthread_mutex_unlock(&entry_mutex);

  CILK_C_UNREGISTER_REDUCER(node_count_parallel);
#else
  entry_point(&args);
#endif

  char bms[MAX_CHARS_IN_MOVE];
  move_to_str(bestMoveSoFar, bms, MAX_CHARS_IN_MOVE);
  snprintf(theMove, MAX_CHARS_IN_MOVE, "%s", bms);
  fprintf(OUT, "bestmove %s\n", bms);
  return;
}

// -----------------------------------------------------------------------------
// argparse help
// -----------------------------------------------------------------------------

// print help messages in uci
void help()  {
  printf("eval      - Evaluate current position.\n");
  printf("display   - Display current board state.\n");
  printf("generate  - Generate all possible moves.\n");
  printf("go        - Search from current state.  Possible arguments are:\n");
  printf("            depth <depth>:     search until depth <depth>\n");
  printf("            time <time_limit>: search assume you have <time> amount of time\n");
  printf("                               for the whole game.\n");
  printf("            inc <time_inc>:    set the fischer time increment for the search\n");
  printf("            Both time arguments are specified in milliseconds.\n");
  printf("            Sample usage: \n");
  printf("                go depth 4: search until depth 4\n");
  printf("help      - Display help (this info).\n");
  printf("isready   - Ask if the UCI engine is ready, if so it echoes \"readyok\".\n");
  printf("            This is mainly used to synchronize the engine with the GUI.\n");
  printf("move      - Make a move for current player.\n");
  printf("            Sample usage: \n");
  printf("                move j0j1: move a piece from j0 to j1\n");
  printf("perft     - Output the number of possible moves upto a given depth.\n");
  printf("            Used to verify move the generator.\n");
  printf("            Sample usage: \n");
  printf("                depth 3: generate all possible moves for depth 1--3\n");
  printf("position  - Set up the board using the fenstring given.  Possible arguments are:\n");
  printf("            startpos:     set up the board with default starting position.\n");
  printf("            endgame:      set up the board with endgame configuration.\n");
  printf("            fen <string>: set up the board using the given fenstring <string>.\n");
  printf("                          See doc/engine-interface.txt for more info on fen notation.\n");
  printf("            Sample usage: \n");
  printf("                position endgame: set up the board so that only kings remain\n");
  printf("quit      - Quit this program\n");
  printf("setoption - Set configuration options used in the engine, the format is: \n");
  printf("            setoption name <name> value <val>.\n");
  printf("            Use the comment \"uci\" to see possible options and their current values\n");
  printf("            Sample usage: \n");
  printf("                setoption name fut_depth value 4: set fut_depth to 4\n");
  printf("uci       - Display UCI version and options\n");
  printf("\n");
}

// Get next token in s[] and put into token[]. Strips quotes.
// Side effects modify ps[].
int parse_string_q(char* s, char* token[]) {
  int token_count = 0;
  parse_state_t state = NONWHITESPACE_STARTS;

  while (*s != '\0') {
    switch (state) {
    case NONWHITESPACE_STARTS:
      switch (*s) {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        *s = '\0';
        break;
      case '"':
        state = QUOTE_ENDS;
        *s = '\0';
        if (*(s + 1) == '\0') {
          fprintf(stderr, "Input parse error: no end of quoted string\n");
          return 0;  // Parse error
        }
        token[token_count++] = s + 1;
        break;
      default:  // nonwhitespace, nonquote
        state = WHITESPACE_ENDS;
        token[token_count++] = s;
      }
      break;

    case WHITESPACE_ENDS:
      switch (*s) {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        state = NONWHITESPACE_STARTS;
        *s = '\0';
        break;
      case '"':
        fprintf(stderr, "Input parse error: misplaced quote\n");
        return 0;  // Parse error
        break;
      default:     // nonwhitespace, nonquote
        break;
      }
      break;

    case QUOTE_ENDS:
      switch (*s) {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        break;
      case '"':
        state = NONWHITESPACE_STARTS;
        *s = '\0';
        if (*(s + 1) != '\0' && *(s + 1) != ' ' &&
            *(s + 1) != '\t' && *(s + 1) != '\n' && *(s + 1) != '\r') {
          fprintf(stderr, "Input parse error: quoted string must be followed by white space\n");
          fprintf(stderr, "ASCII char: %d\n", (int) * (s + 1));
          return 0;  // Parse error
        }
        break;
      default:  // nonwhitespace, nonquote
        break;
      }
      break;
    }
    s++;
  }
  if (state == QUOTE_ENDS) {
    fprintf(stderr, "Input parse error: no end quote on quoted string\n");
    return 0;  // Parse error
  }

  return token_count;
}


void init_options() {
  for (int j = 0; iopts[j].name[0] != 0; j++) {
    assert(iopts[j].min <= iopts[j].dfault);
    assert(iopts[j].max >= iopts[j].dfault);
    *iopts[j].var = iopts[j].dfault;
  }
}

void print_options() {
  for (int j = 0; iopts[j].name[0] != 0; j++) {
    printf("option name %s type spin value %d default %d min %d max %d\n",
           iopts[j].name,
           *iopts[j].var,
           iopts[j].dfault,
           iopts[j].min,
           iopts[j].max);
  }
  return;
}

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  position_t* gme = (position_t*) malloc(sizeof(position_t) * MAX_PLY_IN_GAME);

  setbuf(stdout, NULL);
  setbuf(stdin, NULL);

  OUT = stdout;

  init_options();
  init_zob();


  ///////////////////////////////////////////////////////////////////////////

  // generate some opening positions
#ifdef GEN_OPENINGS
  {
    // NOLINT(whitespace/braces)
    // target at least 4000 opening lines, any that turn out to be non-unique will get removed
#define BOOKLINES 100

    // XXX start with a shallow exploration to have at least one book
    // fast depth=10 and time=5 seconds
#define GENOPENING_DEPTH (MAX_PLY_IN_SEARCH - 1)
    // #define GENOPENING_TIME 4500  // up to 5 seconds
#define GENOPENING_TIME 2000  // up to 10 seconds

    // slow depth=100 time=26 seconds
    // #define GENOPENING_DEPTH (MAX_PLY_IN_SEARCH - 1)
    // #define GENOPENING_TIME 25500  // up to 26 seconds

#define MAX_BOOKMOVES 6
#define MAX_MOVE_STRLEN 5*(MAX_BOOKMOVES+1)
    char opn[MAX_MOVE_STRLEN];
    // progressively shorter depths maybe, or just keep searching?
    int depth = GENOPENING_DEPTH;

    tt_make_hashtable(HASH);    // initial hash table
    int lines;
    for (lines = 0; lines < BOOKLINES; lines++) {
    try_again:
      tt_age_hashtable();
      fen_to_pos(&gme[0], "");

      opn[0] = 0;

      RANDOMIZE = 5 + myrand() % 40;
      double tme = 500.0 + (myrand() % GENOPENING_TIME);

      for (int i = 0; i < MAX_BOOKMOVES; i++) {
        printf("--- new search for line %d, move %d ---\n", lines, i);
        UciBeginSearch(&gme[i], depth, tme);

        strncat(opn, theMove, MAX_MOVE_STRLEN - strlen(opn) - 1);
        strncat(opn, " ", MAX_MOVE_STRLEN - strlen(opn) - 1);
        // printf("so far: %s\n", opn);
        victims_t victims = make_from_string(&gme[i], &gme[i + 1], theMove);
        if (is_KO(victims)) {
          fprintf(OUT, "Illegal move %s.\n", theMove);
          assert(false);
        }
        printf("\n");
        if (victims.zapped_count > 0 &&
            ptype_of(victims.zapped[victims.zapped_count - 1]) == KING) {
          // goto is not harmful here since we must break nested loops.
          goto try_again;  // If King zapped, don't keep playing.
        }
      }
      printf("OPEN: %s\n", opn);
      fflush(stdout);
    }
    fprintf(stderr, "Done gen opening %d lines.\n", lines);
  }
#endif /* GEN_OPENINGS */

  ///////////////////////////////////////////////////////////////////////////



  char** tok = (char**) malloc(sizeof(char*) * MAX_CHARS_IN_TOKEN * MAX_PLY_IN_GAME);
  int    ix = 0;  // index of which position we are operating on

  // input string - last message from UCI interface
  // big enough to support 4000 moves
  char* istr = (char*) malloc(sizeof(char) * 24000);

  tt_make_hashtable(HASH);   // initial hash table
  fen_to_pos(&gme[ix], "");  // initialize with an actual position

  while (true) {
    int n;

    if (fgets(istr, 20478, stdin) != NULL) {
      int token_count = parse_string_q(istr, tok);

      if (token_count == 0) {  // no input
        continue;
      }

      if (strcmp(tok[0], "quit") == 0) {
        break;
      }

      if (strcmp(tok[0], "position") == 0) {
        n = 0;
        if (token_count < 2) {  // no input
          fprintf(OUT, "Second argument required.  Use 'help' to see valid commands.\n");
          continue;
        }

        if (strcmp(tok[1], "startpos") == 0) {
          ix = 0;
          fen_to_pos(&gme[ix], "");
          n = 2;
        } else if (strcmp(tok[1], "endgame") == 0) {
          ix = 0;
          if (BOARD_WIDTH == 10) {
            fen_to_pos(&gme[ix], "ss9/10/10/10/10/10/10/10/10/9NN W");
          } else if (BOARD_WIDTH == 8) {
            fen_to_pos(&gme[ix], "ss7/8/8/8/8/8/8/7NN W");
          }
          n = 2;
        } else if (strcmp(tok[1], "fen") == 0) {
          if (token_count < 3) {  // no input
            fprintf(OUT, "Third argument (the fen string) required.\n");
            continue;
          }
          ix = 0;
          fen_to_pos(&gme[ix], tok[2]);
          n = 3;
        }

        int save_ix = ix;
        if (token_count > n + 1) {
          for (int j = n + 1; j < token_count; j++) {
            victims_t victims = make_from_string(&gme[ix], &gme[ix + 1], tok[j]);
            if (is_ILLEGAL(victims)) {
              fprintf(OUT, "info string Move %s is illegal\n", tok[j]);
              ix = save_ix;
              // breaks multiple loops.
              goto next_command;
            } else {
              ix++;
            }
          }
        }

      next_command:
        continue;
      }

      if (strcmp(tok[0], "move") == 0) {
        victims_t victims = make_from_string(&gme[ix], &gme[ix + 1], tok[1]);
        if (token_count < 2) {  // no input
          fprintf(OUT, "Second argument (move positon) required.\n");
          continue;
        }
        if (is_KO(victims)) {
          fprintf(OUT, "Illegal move %s\n", tok[1]);
        } else {
          ix++;
          display(&gme[ix]);
        }
        continue;
      }

      if (strcmp(tok[0], "uci") == 0) {
        // TODO(you): Change the name & version once you start modifying the code!
        printf("id name %s version %s\n", "Leiserchess", VERSION);
        printf("id author %s\n",
               "Don Dailey, Charles E. Leiserson, and the staff of MIT 6.172 Fall 2012");
        print_options();
        printf("uciok\n");
        continue;
      }

      if (strcmp(tok[0], "isready") == 0) {
        printf("readyok\n");
        continue;
      }

      if (strcmp(tok[0], "setoption") == 0) {
        int sostate = 0;
        char  name[MAX_CHARS_IN_TOKEN];
        char  value[MAX_CHARS_IN_TOKEN];

        strncpy(name, "", MAX_CHARS_IN_TOKEN);
        strncpy(value, "", MAX_CHARS_IN_TOKEN);

        for (int i = 1; i < token_count; i++) {
          if (strcmp(tok[i], "name") == 0) {
            sostate = 1;
            continue;
          }
          if (strcmp(tok[i], "value") == 0) {
            sostate = 2;
            continue;
          }
          if (sostate == 1) {
            // we subtract 1 from the length to account for the
            // additional terminating '\0' that strncat appends
            strncat(name, " ", MAX_CHARS_IN_TOKEN - strlen(name) - 1);
            strncat(name, tok[i], MAX_CHARS_IN_TOKEN - strlen(name) - 1);
            continue;
          }

          if (sostate == 2) {
            strncat(value, " ", MAX_CHARS_IN_TOKEN - strlen(value) - 1);
            strncat(value, tok[i], MAX_CHARS_IN_TOKEN - strlen(value) - 1);
            if (i + 1 < token_count) {
              strncat(value, " ", MAX_CHARS_IN_TOKEN - strlen(value) - 1);
              strncat(value, tok[i + 1], MAX_CHARS_IN_TOKEN - strlen(value) - 1);
              i++;
            }
            continue;
          }
        }

        lower_case(name);
        lower_case(value);

        // see if option is in the configurable integer parameters
        {
          bool recognized = false;
          for (int j = 0; iopts[j].name[0] != 0; j++) {
            char loc[MAX_CHARS_IN_TOKEN];

            snprintf(loc, MAX_CHARS_IN_TOKEN, "%s", iopts[j].name);
            lower_case(loc);
            if (strcmp(name + 1, loc) == 0) {
              recognized = true;
              int v = strtol(value + 1, (char**)NULL, 10);
              if (v < iopts[j].min) {
                v = iopts[j].min;
              }
              if (v > iopts[j].max) {
                v = iopts[j].max;
              }
              printf("info setting %s to %d\n", iopts[j].name, v);
              *(iopts[j].var) = v;

              if (strcmp(name + 1, "hash") == 0) {
                tt_resize_hashtable(HASH);
                printf("info string Hash table set to %d records of "
                       "%zu bytes each\n",
                       tt_get_num_of_records(), tt_get_bytes_per_record());
                printf("info string Total hash table size: %zu bytes\n",
                       tt_get_num_of_records() * tt_get_bytes_per_record());
              }
              break;
            }
          }
          if (!recognized) {
            fprintf(OUT, "info string %s not recognized\n", name + 1);
          }
          continue;
        }
      }

      if (strcmp(tok[0], "help") == 0) {
        help();
        continue;
      }

      if (strcmp(tok[0], "display") == 0) {
        display(&gme[ix]);
        continue;
      }

      sortable_move_t  lst[MAX_NUM_MOVES];
      if (strcmp(tok[0], "generate") == 0) {
        int num_moves = generate_all(&gme[ix], lst, true);
        for (int i = 0; i < num_moves; ++i) {
          char buf[MAX_CHARS_IN_MOVE];
          move_to_str(get_move(lst[i]), buf, MAX_CHARS_IN_MOVE);
          printf("%s ", buf);
        }
        printf("\n");
        continue;
      }

      if (strcmp(tok[0], "eval") == 0) {
        if (token_count == 1) {  // evaluate current position
          score_t score = eval(&gme[ix], true);
          fprintf(OUT, "info score cp %d\n", score);
        } else {  // get and evaluate move
          victims_t victims = make_from_string(&gme[ix], &gme[ix + 1], tok[1]);
          if (is_KO(victims)) {
            fprintf(OUT, "Illegal move %s\n", tok[1]);
          } else {
            // evaluated from opponent's pov
            score_t score = - eval(&gme[ix + 1], true);
            fprintf(OUT, "info score cp %d\n", score);
          }
        }
        continue;
      }

      if (strcmp(tok[0], "go") == 0) {
        double tme = 0.0;
        double inc = 0.0;
        int    depth = INF_DEPTH;
        double goal = INF_TIME;

        // process various tokens here
        for (int n = 1; n < token_count; n++) {
          if (strcmp(tok[n], "depth") == 0) {
            n++;
            depth = strtol(tok[n], (char**)NULL, 10);
            continue;
          }
          if (strcmp(tok[n], "time") == 0) {
            n++;
            tme = strtod(tok[n], (char**)NULL);
            continue;
          }
          if (strcmp(tok[n], "inc") == 0) {
            n++;
            inc = strtod(tok[n], (char**)NULL);
            continue;
          }
        }

        if (depth < INF_DEPTH) {
          UciBeginSearch(&gme[ix], depth, INF_TIME);
        } else {
          goal = tme * 0.02;   // use about 1/50 of main time
          goal += inc * 0.80;  // use most of increment
          // sanity check,  make sure that we don't run ourselves too low
          if (goal * 10 > tme) {
            goal = tme / 10.0;
          }

          UciBeginSearch(&gme[ix], INF_DEPTH, goal);
        }
        continue;
      }

      if (strcmp(tok[0], "perft") == 0) {  // Test move generator
        // Correct output (for a 10x10 board):
        // perft  1 78
        // perft  2 6084
        // perft  3 473371
        // perft  4 36823027

        int depth = 4;
        if (token_count >= 2) {  // Takes a depth argument to test deeper
          depth = strtol(tok[1], (char**)NULL, 10);
        }
        do_perft(gme, depth, 0);
        continue;
      }

      printf("Illegal command.  Use 'help' to see possible options.\n");
      continue;
    }
  }
  tt_free_hashtable();

  return 0;
}
