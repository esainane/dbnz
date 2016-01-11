/*
 * Memory inspector for libdbnz
 */
#include "dbnz.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static const DBNZ_CELL_TYPE *current_state;
static DBNZ_CELL_TYPE *last_dump_state;

static size_t last[64];
static unsigned int last_step;
static size_t last_plen;
static size_t last_dump_plen;

#define SQUASH_THRESHOLD 5

static void print_values(size_t count, DBNZ_CELL_TYPE value) {
  if (count < SQUASH_THRESHOLD) {
    while (count--) {
      printf("%" DBNZ_CELL_FMT " ", value);
    }
  } else {
    printf("%" DBNZ_CELL_FMT " ... (%" PRIuMAX " repeats of %" DBNZ_CELL_FMT " omitted) ... ", value, (uintmax_t) (count - 1), value);
  }
}

/* Print out state, summarising long runs of the same value */
static void dump_mem_simple(const DBNZ_CELL_TYPE *state, size_t plen) {
  DBNZ_CELL_TYPE value = 0;
  size_t count = 0;
  size_t i;
  for (i = 0; i != plen; ++i) {
    if (state[i] == value) {
      ++count;
      continue;
    }
    print_values(count, value);
    count = 1;
    value = state[i];
  }
  print_values(count, value);
}

/* Print out the new state, summarising long runs where values are unchanged */
static void dump_mem_diff(const DBNZ_CELL_TYPE *current_state, const DBNZ_CELL_TYPE *last_dump_state, size_t plen) {
  size_t unchanged_count = 0;
  size_t lookahead, dump_pos = 0; /* dump_pos is the index of the next cell that needs to be printed, inclusive */
  /*
   * The lookahead scans for similarities between the states.
   *
   * We need to wait until we're necessarily past the end of a run before flushing, to ensure that our runs need only know about their inner contents for repeat summarization.
   *
   * Whenever the threshold for squashing output is reached, we're about to summarize the similarities, and any pending differences are flushed.
   * Whenever we find a differing cell, if we need to summarize the similarities, we immediately do so.
   * Less than SQUASH_THRESHOLD common cells in a row are simply treated as an extension of the enclosing differing cells, to be dumped verbatim.
   */
  for (lookahead = 0; lookahead != plen; ++lookahead) {
    if (current_state[lookahead] == last_dump_state[lookahead]) {
      /* Accumulate similarities. If we've had a sufficiently long run of similarities, flush any depending differences, since we'll summarize our similarities when this run is over. */
      if (++unchanged_count == SQUASH_THRESHOLD) {
        size_t pending = lookahead - dump_pos;
        dump_mem_simple(current_state + dump_pos, pending - (SQUASH_THRESHOLD - 1));
        dump_pos = lookahead - SQUASH_THRESHOLD;
      }
      continue;
    }
    /* Accumulate differences. If anything insignificant was unchanged, we ignore it and merge it into our current run. */
    if (unchanged_count < SQUASH_THRESHOLD) {
      unchanged_count = 0;
      continue;
    }
    /* Otherwise, if there was a significant run unchanged, summarize it now. */
    printf("... (%" PRIuMAX " common cells omitted) ... ", unchanged_count);
    dump_pos = lookahead;
    unchanged_count = 0;
  }
  /* Is our final flush for our similarities, or our differences? */
  if (unchanged_count < SQUASH_THRESHOLD) {
    dump_mem_simple(current_state + dump_pos, plen - 1 - dump_pos);
  } else {
    printf("... (%" PRIuMAX " common cells omitted) ... ", unchanged_count);
  }
}

static void dump_mem(const DBNZ_CELL_TYPE *state, size_t plen) {
  printf("Memory dump: state length: %" PRIuMAX "\n", (uintmax_t) plen);
  if (last_dump_plen != plen) {
    dump_mem_simple(state, plen);
    last_dump_plen = plen;
    last_dump_state = realloc(last_dump_state, sizeof(DBNZ_CELL_TYPE) * plen);
  } else {
    dump_mem_diff(state, last_dump_state, plen);
  }
  puts("");
  memcpy(last_dump_state, state, sizeof(DBNZ_CELL_TYPE) * plen);
}

static void stepcallback(const DBNZ_CELL_TYPE *state, size_t plen, size_t cursor, unsigned int step) {
  if (!step)
    dump_mem(state, plen);
#ifdef HEARTBEAT
  if (!(step & 1023))
    printf("Step %u, cursor %" PRIuMAX "\n", step, (uintmax_t) cursor);
#endif
  current_state = state;
  last_plen = plen;
  last_step = step;
  last[step & 63] = cursor;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Please specify a program to run.\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "./viz statefile\n");
    exit(1);
  }
  int ret = dbnz_file_bootstrap(argv[1], &stepcallback);
  printf("Execution finished returning %d after %u steps.\n", ret, last_step);
  dump_mem(current_state, last_plen);
  return 0;
}
