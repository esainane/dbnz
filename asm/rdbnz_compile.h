
#ifndef RDBNZ_COMPILE_H
#define RDBNZ_COMPILE_H

struct dbnz_compile_state {
  size_t proglen;
  size_t constant_num;
  size_t constant_max;
  size_t *constants;
  size_t exec_num;
  size_t exec_max;
  struct dbnz_rval **execs;
  size_t visited_num;
  size_t visited_max;
  const char **visited;
  size_t output_num;
  size_t output_max;
  size_t *output;
  size_t stack_len;
  struct locals *globals;
};

/* Find or add a constant to the constant pool */
size_t dbnz_pool_constant(size_t value, struct dbnz_compile_state *s);
/* Fully compile the program */
void dbnz_compile_program(struct dbnz_macro *macros, struct dbnz_statementlist stmts, size_t memory, struct dbnz_compile_state *s);

#endif /* RDBNZ_COMPILE_H */
