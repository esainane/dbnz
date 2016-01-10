
#ifndef RDBNZ_COMPILE_H
#define RDBNZ_COMPILE_H

struct dbnz_compile_state {
  /* Number of cells available to the VM. */
  size_t proglen;
  /* List of constants. */
  size_t constant_num;
  size_t constant_max;
  size_t *constants;
  /* List of executable cells.*/
  size_t exec_num;
  size_t exec_max;
  struct dbnz_rval **execs;
  /* List of all contexts we are currently inside of. */
  size_t visited_num;
  size_t visited_max;
  const char **visited;
  /* Length of registers used by enclosing contexts. */
  size_t stack_top;
  /* Length of registers used by the current context. */
  size_t reg_num;
  /* Locals from the top level context - a list of labels. */
  struct locals *globals;
};

/* Find or add a constant to the constant pool */
size_t dbnz_pool_constant(size_t value, struct dbnz_compile_state *s);
/* Fully compile the program */
void dbnz_compile_program(struct dbnz_macro *macros, struct dbnz_statementlist stmts, size_t memory, struct dbnz_compile_state *s);

#endif /* RDBNZ_COMPILE_H */
