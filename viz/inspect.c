/*
 * Memory inspector for libdbnz
 */
#include "dbnz.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

static const DBNZ_CELL_TYPE *current_state;

static size_t last[64];
static unsigned int last_step;
static size_t last_plen;

static void print_values(size_t count, DBNZ_CELL_TYPE value) {
  if (count < 5) {
    while (count--) {
      printf("%" DBNZ_CELL_FMT " ", value);
    }
  } else {
    printf("%" DBNZ_CELL_FMT " ... (%" PRIuMAX " repeats of %" DBNZ_CELL_FMT " omitted) ... ", value, (uintmax_t) (count - 1), value);
  }
}

static void dump_mem(const DBNZ_CELL_TYPE *state, size_t plen) {
  printf("Memory dump: plen %" PRIuMAX "\n", (uintmax_t) plen);
  DBNZ_CELL_TYPE value;
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
  puts("");
}

static void stepcallback(const DBNZ_CELL_TYPE *state, size_t plen, size_t cursor, unsigned int step) {
  if (!step)
    dump_mem(state, plen);
  if (!(step & 1023))
    printf("Step %u, cursor %" PRIuMAX "\n", step, (uintmax_t) cursor);
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
  printf("Execution finished after %u steps.\n.", last_step);
  dump_mem(current_state, last_plen);
  return 0;
}
