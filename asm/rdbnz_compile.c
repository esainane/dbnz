
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "rdbnz.h"
#include "rdbnz_compile.h"

struct locals {
  size_t num;
  size_t max;
  char **keys;
  struct dbnz_rval **vals;
};

struct deferred {
  size_t num;
  size_t max;
  struct dbnz_rval ***idents;
};

static void process_idents(struct dbnz_compile_state *s, struct deferred *deferred, struct locals *locals);
static void mark_idents(struct deferred *deferred, struct dbnz_rval **rval);

static void add_locals(struct dbnz_param *param, struct dbnz_rval *args, struct locals *locals, struct deferred *deferred, const char *target_id, const char *context) {
  memset(locals, '\0', sizeof(struct locals));
  for (; param && args; param = param->next, args = args->next) {
    if (!locals->max) {
      locals->max += 50;
      locals->keys = realloc(locals->keys, sizeof(char *) * locals->max);
      locals->vals = realloc(locals->vals, sizeof(size_t) * locals->max);
    } else if (locals->num == locals->max) {
      fprintf(stderr, "Internal error: Hit locals limit, aborting! Bug the author to get rid of their workaround.\n");
      exit(1);
    }
    locals->keys[locals->num] = param->id;
    locals->vals[locals->num] = args;
    mark_idents(deferred, &locals->vals[locals->num]); /* FIXME This is not safe with more than <max increment> locals! */
    ++locals->num;
  }
  if (param) {
    int needed;
    for (needed = 0; ++needed, param = param->next; ) ;
    fprintf(stderr, "Macro '%s' called from %s requires %d more arguments!\n", target_id, context, needed);
    exit(1);
  }
  if (args) {
    int extra;
    for (extra = 0; ++extra, args = args->next; ) ;
    fprintf(stderr, "Macro '%s' called from %s requires %d fewer arguments!\n", target_id, context, extra);
    exit(1);
  }
}

/* XXX: God function. Split me. */
static void process_stmts(struct dbnz_macro *macros, struct dbnz_statementlist *stmtlist, struct dbnz_compile_state *s, struct locals locals, const char *context) {
  size_t start = s->exec_num;
  /* If we're the top level, our local labels are globals */
  printf("Begin context: %s.\n", context);
  if (!s->globals) {
    s->globals = &locals;
  } else {
    if (s->visited_num == s->visited_max) {
      s->visited = realloc(s->visited, sizeof(char *) * (s->visited_max += 10));
    }
    s->visited[s->visited_num++] = context;
  }
  struct dbnz_statement *stmts = stmtlist->list;
  struct deferred deferred;
  memset(&deferred, '\0', sizeof(struct deferred));
  /* Our statement list must have at least one statement to be valid */
  do {
    switch (stmts->type) {
    case STMT_LABEL:
      /* List management */
      if (locals.num == locals.max) {
        locals.max += 10;
        locals.keys = realloc(locals.keys, sizeof(char *) * locals.max);
        locals.vals = realloc(locals.vals, sizeof(size_t) * locals.max);
      }
      /* Add a new key-value pair to the list */
      /* XXX: Low hanging fruit: The locals of each function are resolved at the end, lookups could be accelerated by sorting locals as soon as it starts to matter */
      locals.keys[locals.num] = stmts->u.n;
      locals.vals[locals.num] = malloc(sizeof(struct dbnz_rval));
      memset(locals.vals[locals.num], '\0', sizeof(struct dbnz_rval));
      locals.vals[locals.num]->u.v = s->exec_num;
      locals.vals[locals.num]->type = RVAL_LABEL;
      ++locals.num;
      break;
    case STMT_CALL: {
      char *target_id = stmts->u.c.target;
      /* Find our target macro */
      struct dbnz_macro *target;
      for (target = macros; target && strcmp(target->id, target_id); target = target->next) ;
      if (!target) {
        fprintf(stderr, "Unknown macro '%s' called from %s!\n", target_id, context);
        exit(1);
      }
      /* Sanity check: Make sure we're not recursing */
      size_t i;
      for (i = 0; i != s->visited_num && strcmp(s->visited[i], target_id); ++i) ;
      if (i != s->visited_num) {
        fprintf(stderr, "Attempted to make a recursive call in %s (%" PRIuMAX "/%" PRIuMAX " calls from top level): ", context, (uintmax_t) i, (uintmax_t) s->visited_num);
        do {
          fprintf(stderr, "%s -> ", s->visited[i]);
        } while (++i != s->visited_num);
        fprintf(stderr, "%s. All functions are currently inlined, direct or mutual recursion is not yet supported.\n", target_id);
        exit(1);
      }
      /* Establish locals and process */
      struct locals target_locals;
      add_locals(target->argv, stmts->u.c.argv, &target_locals, &deferred, target_id, context);
      process_stmts(macros, &target->statements, s, target_locals, target_id);
    } break;
    case STMT_DBNZ:
      if (!s->exec_max) {
          s->execs = realloc(s->execs, sizeof(struct dbnz_rval *) * (s->exec_max += 250));
      } else if (s->exec_num + 2 > s->exec_max) {
          fprintf(stderr, "Internal error: Hit execs limit, aborting! Bug the author to get rid of their workaround.\n");
          exit(1);
      }
      s->execs[s->exec_num] = stmts->u.d.target;
      mark_idents(&deferred, &s->execs[s->exec_num]); /* FIXME Not safe, as above */
      ++s->exec_num;
      s->execs[s->exec_num] = stmts->u.d.jump;
      mark_idents(&deferred, &s->execs[s->exec_num]); /* FIXME Not safe, as above */
      ++s->exec_num;
      break;
    }
    stmts = stmts->next;
  } while (stmts);
  --s->visited_num;
  printf("Context %s from cells (prog + %" PRIuMAX ") to (prog + %" PRIuMAX ").\n", context, (uintmax_t) start, (uintmax_t) s->exec_num);
  process_idents(s, &deferred, &locals);
}

static void init_output(struct dbnz_compile_state *s, size_t memory) {
  s->output_max = memory;
  s->output = malloc(sizeof(size_t *) * memory);
}

static void write_constants(struct dbnz_compile_state *s) {
  size_t i;
  for (i = 0; i != s->constant_num; ++i) {
    s->output[s->output_num++] = s->constants[i];
  }
}

/* Simplify an rval during the initial pass */
static void mark_idents(struct deferred *deferred, struct dbnz_rval **rval) {
  switch ((*rval)->type) {
  case RVAL_ADD:  /* Can only recurse and resolve parameters */
  case RVAL_SUB:
    mark_idents(deferred, &(*rval)->u.p.lhs);
    mark_idents(deferred, &(*rval)->u.p.rhs);
    break;
  case RVAL_IDENTIFIER: /* These are layers of indirection to rvals that will be resolved in preprocessing. */
    if (deferred->num == deferred->max) {
        deferred->idents = realloc(deferred->idents, sizeof(struct dbnz_rval *) * (deferred->max += 15));
    }
    deferred->idents[deferred->num++] = rval;
    break;
  case RVAL_CONSTANT:
  case RVAL_THIS:
  case RVAL_DATA:
  case RVAL_STACK:
  case RVAL_INTEGER:
  case RVAL_LABEL:
    break;
    /* Nothing to do */
  }
}

static struct dbnz_rval *find_local(const char *id, struct locals *locals) {
  int i;
  for (i = 0; i != locals->num; ++i) {
    if (!strcmp(id, locals->keys[i])) {
      return locals->vals[i];
    }
  }
  return 0;
}

static void process_idents(struct dbnz_compile_state *s, struct deferred *deferred, struct locals *locals) {
  int i;
  for (i = 0; i != deferred->num; ++i) {
    if ((*deferred->idents[i])->type != RVAL_IDENTIFIER) {
      fprintf(stderr, "Internal error: Unexpected type in ident resolution, this should never happen!\n");
      exit(1);
    }
    const char *id = (*deferred->idents[i])->u.n;
    struct dbnz_rval *ref = find_local(id, locals);
    if (!ref && locals != s->globals) {
      ref = find_local(id, s->globals);
    }
    if (!ref) {
      fprintf(stderr, "Unable to process identifier '%s'!\n", id);
      exit(1);
    }
    *deferred->idents[i] = ref;
    if (ref->type == RVAL_IDENTIFIER) {
      fprintf(stderr, "Internal error: ident is still ident (%s) after resolution\n", ref->u.n);
    }
  }
}

/* Fully resolve an rval */
static size_t resolve_rval(struct dbnz_compile_state *s, struct dbnz_rval *rval, size_t where) {
  switch (rval->type) {
  case RVAL_CONSTANT:
    fprintf(stderr, "Internal error: Found unresolved constant in rval during final compilation pass, this should never happen!\n");
    exit(1);
  case RVAL_STACK:
    rval->u.v += s->stack_len;
    break;
  case RVAL_DATA:
    rval->u.v = s->constant_num + s->exec_num;
    break;
  case RVAL_IDENTIFIER:
    fprintf(stderr, "Internal error: Found unresolved identifier '%s' in rval at cell '%" PRIuMAX "' (prog + '%" PRIuMAX "') uring final compilation pass, this should never happen!\n", rval->u.n, (uintmax_t) where, (uintmax_t) (where - s->constant_num));
    exit(1);
  case RVAL_ADD:
    resolve_rval(s, rval->u.p.lhs, where);
    resolve_rval(s, rval->u.p.rhs, where);
    rval->u.v = rval->u.p.lhs->u.v + rval->u.p.rhs->u.v;
    break;
  case RVAL_SUB:
    resolve_rval(s, rval->u.p.lhs, where);
    resolve_rval(s, rval->u.p.rhs, where);
    rval->u.v = rval->u.p.lhs->u.v - rval->u.p.rhs->u.v;
    break;
  case RVAL_THIS:
    rval->u.v = where;
    break;
  case RVAL_INTEGER:
  case RVAL_LABEL:
    /* Nothing to do */
    break;
  }
  rval->type = RVAL_INTEGER;
}

/* Write the program to state output, fully resolving all expressions */
static void write_program(struct dbnz_compile_state *s) {
  size_t i;
  for (i = 0; i != s->exec_num; ++i) {
    s->output[s->output_num++] = resolve_rval(s, s->execs[i], i + s->constant_num);
  }
}

/* Find or add a constant to the constant pool */
size_t dbnz_pool_constant(size_t value, struct dbnz_compile_state *s) {
  size_t i;
  for (i = 0; i != s->constant_num; ++i) {
    if (s->constants[i] == value) {
      return i;
    }
  }
  if (s->constant_num == s->constant_max) {
    s->constants = realloc(s->constants, sizeof(size_t) * (s->constant_max += 10));
  }
  s->constants[s->constant_num] = value;
  return s->constant_num++;
}

void dbnz_compile_program(struct dbnz_macro *macros, struct dbnz_statementlist stmtlist, size_t memory, struct dbnz_compile_state *s) {
  struct locals l;
  memset(&l, '\0', sizeof(struct locals));
  process_stmts(macros, &stmtlist, s, l, "<top level>");

  init_output(s, memory);

  write_constants(s);
  write_program(s);
}
