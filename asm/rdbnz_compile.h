
#ifndef RDBNZ_COMPILE_H
#define RDBNZ_COMPILE_H

#include "rdbnz.h"

/* Fully compile the program */
struct dbnz_compile_state * dbnz_compile_program(struct dbnz_statementlist *stmtlist, struct dbnz_macro *macros, size_t memory);

#endif /* RDBNZ_COMPILE_H */
