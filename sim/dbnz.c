/*
 * Core libdbnz library
 */

#include "dbnz.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

static void dbnz_bounds_hcf(const char *what, unsigned int step, size_t cursor, size_t len) {
  fprintf(stderr, "Invalid access (%s) at %" PRIuMAX  "; upper bound (exclusive) is %" PRIuMAX, what, (uintmax_t) cursor, (uintmax_t) len);
  exit(1);
}

void dbnz_bootstrap(DBNZ_CELL_TYPE *state, size_t plen, void (*stepcallback)(const DBNZ_CELL_TYPE *state, size_t cursor, unsigned int step)) {
  size_t cursor = 0; /* Start at the first instruction */
  int running = 1;
  unsigned int step = 0;
  do {
    if (cursor >= plen) {
      dbnz_bounds_hcf("Execution cursor", step, cursor, plen);
    }
    size_t target = state[cursor];
    if (target >= plen) {
      dbnz_bounds_hcf("Decrement target", step, cursor, plen);
    }
    --state[target];
    state[target] &= DBNZ_CELL_MASK;
    /* Branch if non zero, otherwise move to the next line */
    cursor = state[target] ? state[cursor + 1] : cursor + 2;
    if (stepcallback) {
      stepcallback(state, cursor, step);
    }
    ++step;
  } while (running);
}
