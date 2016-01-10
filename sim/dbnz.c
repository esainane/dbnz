/*
 * Core libdbnz library
 */

#include "dbnz.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static void dbnz_bounds_hcf(const char *what, unsigned int step, size_t cursor, size_t len) {
  fprintf(stderr, "Invalid access (%s) at %" PRIuMAX  "; upper bound (exclusive) is %" PRIuMAX "\n", what, (uintmax_t) cursor, (uintmax_t) len);
  exit(1);
}


int dbnz_file_bootstrap(const char *filename, void (*stepcallback)(const DBNZ_CELL_TYPE *state, size_t plen, size_t cursor, unsigned int step)) {
  FILE * f = fopen(filename, "r");
  if (!f) {
    fprintf(stderr, "Unable to open file '%s', aborting!\n", filename);
    exit(1);
  }
  /* Terrible workaround for %z not being as widely supported as it should be */
  uintmax_t tmp1, tmp2;
  size_t plen = 0, start = 0;
  /* Read the file header - plen and start. */
  fscanf(f, " %" PRIuMAX  " %" PRIuMAX  " ", &tmp1, &tmp2);
  plen = tmp1, start = tmp2;
  DBNZ_CELL_TYPE *state = malloc(sizeof(DBNZ_CELL_TYPE) * plen);
  memset(state, '\0', sizeof(DBNZ_CELL_TYPE) * plen);
  size_t offset;
  for (offset = 0; offset + 1 < plen; ) {
    /* Read up to two number from the file. */
    size_t pos = offset;
    switch (fscanf(f, "%" PRIuMAX  " %" PRIuMAX, &tmp1, &tmp2)) {
      case 0: {
        /* We couldn't parse anything. Try to figure out what went wrong. */
        char buf[30];
        fscanf(f, " %29[^\n]", buf);
        fprintf(stderr, "Parsing error near (%s), is the input file valid?\n", buf);
        exit(1);
      }
      case EOF:
        return dbnz_bootstrap(state, plen, start, stepcallback);
      case 2:
        state[pos + 1] = tmp2;
        ++offset;
        /* Fall through */
      case 1:
        state[pos    ] = tmp1;
        ++offset;
    }
    /* Ignore everything up until the next newline. */
    fscanf(f, "%*[^\n]*\n");
  }
  fprintf(stderr, "File goes beyond memory available to the environment, limit is %" PRIuMAX  ", aborting!\n", (uintmax_t) plen);
}

int dbnz_bootstrap(DBNZ_CELL_TYPE *state, size_t plen, size_t cursor, void (*stepcallback)(const DBNZ_CELL_TYPE *state, size_t plen, size_t cursor, unsigned int step)) {
  if (cursor & 1) {
    fprintf(stderr, "Invalid starting position %" PRIuMAX  ", aborting!\n");
    exit(1);
  }
  unsigned int step = 0;
  for (;;) {
    if (cursor >= plen) {
      dbnz_bounds_hcf("Execution cursor", step, cursor, plen);
    }
    if (stepcallback) {
      stepcallback(state, plen, cursor, step);
    }
    DBNZ_CELL_TYPE target = state[cursor];
    if (target >= plen) {
      dbnz_bounds_hcf("Decrement target", step, cursor, plen);
    }
  #ifdef SILLY_VERBOSE
    printf("Cursor address: %" PRIuMAX  " decrement target address: %" DBNZ_CELL_FMT " decrement target value: %" DBNZ_CELL_FMT " non-zero jump address: %" DBNZ_CELL_FMT "\n", (uintmax_t) cursor, target, state[target], state[cursor + 1]);
  #endif
    --state[target];
    state[target] &= DBNZ_CELL_MASK;
    /* Branch if non zero, otherwise move to the next line */
    cursor = state[target] ? state[cursor + 1] : cursor + 2;
    if (cursor & 1) {
      return cursor >> 1;
    }
    ++step;
  }
}
