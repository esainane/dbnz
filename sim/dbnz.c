/*
 * Core libdbnz library
 */

#include "dbnz.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static void dbnz_bounds_hcf(const char *what, unsigned int step, size_t cursor, size_t len) {
  fprintf(stderr, "Invalid access (%s) at %" PRIuMAX  "; upper bound (exclusive) is %" PRIuMAX, what, (uintmax_t) cursor, (uintmax_t) len);
  exit(1);
}


int dbnz_file_bootstrap(const char *filename, void (*stepcallback)(const DBNZ_CELL_TYPE *state, size_t cursor, unsigned int step)) {
  FILE * f = fopen(filename, "r");
  if (!f) {
    fprintf(stderr, "Unable to open file '%s', aborting!\n", filename);
    exit(1);
  }
  size_t plen;
  fscanf(f, " %z ", &plen);
  DBNZ_CELL_TYPE *state = malloc(sizeof(DBNZ_CELL_TYPE) * plen);
  memset(state, '\0', sizeof(DBNZ_CELL_TYPE) * plen);
  size_t offset;
  for (offset = 0; offset + 1 < plen; offset += 2) {
    if (EOF == fscanf(f, " %z %z[^\n]*\n", state + offset, state + offset + 1)) {
      break;
    }
  }
  return dbnz_bootstrap(state, plen, stepcallback);
}

int dbnz_bootstrap(DBNZ_CELL_TYPE *state, size_t plen, void (*stepcallback)(const DBNZ_CELL_TYPE *state, size_t cursor, unsigned int step)) {
  size_t cursor = 0; /* Start at the first instruction */
  unsigned int step = 0;
  for (;;) {
    if (cursor >= plen) {
      dbnz_bounds_hcf("Execution cursor", step, cursor, plen);
    }
    if (stepcallback) {
      stepcallback(state, cursor, step);
    }
    size_t target = state[cursor];
    if (target >= plen) {
      dbnz_bounds_hcf("Decrement target", step, cursor, plen);
    }
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
