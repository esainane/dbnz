
#ifndef DBNZ_H
#define DBNZ_H

#include <stdlib.h>
#include <stdint.h>

struct dbnz;

#if !defined(DBNZ_CELL_WIDTH) || !defined(DBNZ_CELL_TYPE)
#define DBNZ_CELL_MASK 0xffff
#define DBNZ_CELL_TYPE uint16_t
#endif

/**
 * Loads in a dbnz file and executes it.
 */
int dbnz_file_bootstrap(const char *filename, void (*stepcallback)(const DBNZ_CELL_TYPE *state, size_t cursor, unsigned int step));

/**
 * @param state Initial state for the DBNZ machine
 * @param plen Program length; number of cells in the provided state
 * @param stepcallback Callback to fire after every step, optional. Parameters are the current state, the in-state offset to the cursor, and a counter for the number of steps taken.
 *
 * @return Program's exit code
 */
int dbnz_bootstrap(DBNZ_CELL_TYPE *state, size_t plen, void (*stepcallback)(const DBNZ_CELL_TYPE *state, size_t cursor, unsigned int step));

#endif /* DBNZ_H */
