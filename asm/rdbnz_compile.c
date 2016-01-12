
#include "rdbnz.h"

#include "rdbnz_pool.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

struct locals {
  size_t num;
  size_t max;
  char **keys;
  struct dbnz_rval **vals;
};

struct deferred {
  size_t num;
  size_t max;
  struct dbnz_rval **idents;
};

/**
 * Deferred processing for statement list finalization.
 *
 * At the end of a statement list, we have a complete picture of references,
 * including post-usage labels that may shadow global labels.
 */

static struct dbnz_rval *find_local(const char *id, struct locals *locals) {
  size_t i;
  for (i = 0; i != locals->num; ++i) {
    if (!strcmp(id, locals->keys[i])) {
      return locals->vals[i];
    }
  }
  return 0;
}

static void finalize_deferred(struct dbnz_compile_state *s, struct deferred *deferred, struct locals *locals) {
  size_t i;
  for (i = 0; i != deferred->num; ++i) {
    if (deferred->idents[i]->type != RVAL_IDENTCOPY) {
      fprintf(stderr, "Internal error: Unexpected type in ident resolution, this should never happen!\n");
      exit(1);
    }
    const char *id = deferred->idents[i]->u.n;
    struct dbnz_rval *ref = find_local(id, locals);
    if (!ref && locals != s->globals) {
      ref = find_local(id, s->globals);
    }
    if (!ref) {
      fprintf(stderr, "Unable to process identifier '%s'!\n", id);
      exit(1);
    }
    *deferred->idents[i] = *ref;
    if (ref->type == RVAL_IDENTIFIER) {
      fprintf(stderr, "Internal error: ident copy is AST data (!?) (%s) after resolution, this should never happen!\n", ref->u.n);
    }
    if (ref->type == RVAL_IDENTCOPY) {
      fprintf(stderr, "Internal error: ident copy is still ident copy %s after resolution, this should never happen!\n", ref->u.n);
    }
  }
}

/**
 * rval processing.
 */

/*
 * Translate AST entities into data suitable for resolution to cell lists.
 * If an rval needs later modification, copy it and track it for deferred
 * processing during the initial pass.
 * Fixed AST elements may be copied directly into the executable space.
 */
static struct dbnz_rval *process_rval(struct dbnz_compile_state *s, struct deferred *deferred, struct dbnz_rval *rval) {
  switch (rval->type) {
  case RVAL_STACK:
    if (rval->u.v > s->reg_num) {
      s->reg_num = rval->u.v;
    }
    struct dbnz_rval *val = malloc(sizeof(struct dbnz_rval));
    *val = *rval;
    val->type = RVAL_INTEGER;
    val->u.v = s->proglen - (s->stack_top + rval->u.v);
    return val;
  case RVAL_ADD:  /* Can only recurse and resolve parameters */
  case RVAL_SUB: {
    struct dbnz_rval *lret, *rret;
    lret = process_rval(s, deferred, rval->u.p.lhs);
    rret = process_rval(s, deferred, rval->u.p.rhs);
    if (lret != rval->u.p.lhs || rret != rval->u.p.rhs) {
      struct dbnz_rval *cpy = malloc(sizeof(struct dbnz_rval));
      *cpy = *rval;
      cpy->u.p.lhs = lret;
      cpy->u.p.rhs = rret;
      return cpy;
    }
    return rval;
  }
  case RVAL_IDENTIFIER: /* These are layers of indirection to rvals that will be resolved in preprocessing. */
    if (deferred->num == deferred->max) {
        deferred->idents = realloc(deferred->idents, sizeof(struct dbnz_rval *) * (deferred->max += 15));
    }
    /* Ensure only one context takes responsibility per copy */
    struct dbnz_rval *cpy = malloc(sizeof(struct dbnz_rval));
    *cpy = *rval;
    cpy->type = RVAL_IDENTCOPY;
    /* We should never see an RVAL_IDENTIFIER outside of statement list processing when we originally process it. */
    /* We should never see an RVAL_IDENTCOPY outside of deferred resolution during statement list finalisation, and final program writing through the RVAL_REF indirection. */
    /* An ident RVAL_REF may be copied around freely, but should always point directly to the authorative RVAL_IDENTCOPY. */
    struct dbnz_rval *ref = malloc(sizeof(struct dbnz_rval));
    *ref = *cpy;
    ref->type = RVAL_REF;
    ref->u.r = cpy;
    /* We'll later modify this copy, resolving it based on local context. */
    deferred->idents[deferred->num++] = cpy;
    return ref;
  case RVAL_IDENTCOPY:
    fprintf(stderr, "Tried to process RVAL_IDENTCOPY not guarded by RVAL_REF, this should never happen!\n");
    exit(1);
  case RVAL_CONSTANT:
    rval->u.v = dbnz_pool_constant(s, rval->u.v);
    rval->type = RVAL_INTEGER;
  case RVAL_REF:
    /* Deliberately do nothing, this will already be handled */
  case RVAL_THIS:
  case RVAL_DATA:
  case RVAL_INTEGER:
  case RVAL_LABEL:
    /* Nothing to do */
    return rval;
  }
}

static void add_locals(struct dbnz_compile_state *s, struct dbnz_param *param, struct dbnz_rval *args, struct locals *locals, struct deferred *deferred, const char *target_id, const char *context) {
  memset(locals, '\0', sizeof(struct locals));
  for (; param && args; param = param->next, args = args->next) {
    if (locals->num == locals->max) {
      locals->max += 50;
      locals->keys = realloc(locals->keys, sizeof(char *) * locals->max);
      locals->vals = realloc(locals->vals, sizeof(struct dbnz_rval *) * locals->max);
    }
    locals->keys[locals->num] = param->id;
    locals->vals[locals->num] = process_rval(s, deferred, args);
    ++locals->num;
  }
  if (param) {
    int needed;
    for (needed = 0; ++needed, param = param->next; ) ;
    fprintf(stderr, "Macro '%s' called from %s requires %d more arguments (expected %" PRIuMAX ")!\n", target_id, context, needed, locals->num + needed);
    exit(1);
  }
  if (args) {
    int extra = 0;
    while (++extra, args = args->next) ;
    fprintf(stderr, "Macro '%s' called from %s requires %d fewer arguments (expected %" PRIuMAX ")!\n", target_id, context, extra, locals->num);
    exit(1);
  }
}

/**
 *  Statement processing.
 */

static void process_stmts(struct dbnz_compile_state *s, struct dbnz_statementlist *stmtlist, struct dbnz_macro *macros, struct locals locals);

static void process_label_stmt(struct dbnz_compile_state *s, struct dbnz_statement *stmts, struct locals *locals) {
  /* List management */
  if (locals->num == locals->max) {
    locals->max += 10;
    locals->keys = realloc(locals->keys, sizeof(char *) * locals->max);
    locals->vals = realloc(locals->vals, sizeof(size_t) * locals->max);
  }
  /* Add a new key-value pair to the list */
  /* XXX: Low hanging fruit: The locals of each function are resolved at the end, lookups could be accelerated by sorting locals as soon as it starts to matter */
  locals->keys[locals->num] = stmts->u.n;
  locals->vals[locals->num] = malloc(sizeof(struct dbnz_rval));
  *locals->vals[locals->num] = (struct dbnz_rval) { .line_no = stmts->line_no, .file_no = stmts->file_no, .type = RVAL_LABEL, .u = { .v = s->exec_num }};
  ++locals->num;
}

static void process_call_stmt(struct dbnz_compile_state *s, struct dbnz_statement *stmts, struct deferred *deferred, struct dbnz_macro *macros) {
  char *target_id = stmts->u.c.target;
  /* Find our target macro */
  struct dbnz_macro *target;
  for (target = macros; target && strcmp(target->id, target_id); target = target->next) ;
  if (!target) {
    fprintf(stderr, "Unknown macro '%s' called from %s!\n", target_id, s->context);
    exit(1);
  }

  /* Sanity check: Make sure we're not recursing */
  size_t i;
  for (i = 0; i != s->visited_num && strcmp(s->visited[i], target_id); ++i) ;
  if (i != s->visited_num) {
    fprintf(stderr, "Attempted to make a recursive call in %s (%" PRIuMAX "/%" PRIuMAX " calls from top level): ", s->context, (uintmax_t) i, (uintmax_t) s->visited_num);
    do {
      fprintf(stderr, "%s -> ", s->visited[i]);
    } while (++i != s->visited_num);
    fprintf(stderr, "%s. All functions are currently inlined, direct or mutual recursion is not yet supported.\n", target_id);
    exit(1);
  }

  /* Establish locals and process */
  struct locals target_locals;
  add_locals(s, target->argv, stmts->u.c.argv, &target_locals, deferred, target_id, s->context);

  size_t stack_top = s->stack_top; /* Sanity check */

  size_t reg_num = s->reg_num; /* Save the number of registers we are using. */
  const char *context = s->context; /* Save the name of the context currently being processed. */
  s->context = target_id;
  s->stack_top += reg_num;
  process_stmts(s, &target->statements, macros, target_locals);
  s->reg_num = reg_num;
  s->stack_top -= reg_num;
  s->context = context;

  /* Perform our sanity check */
  if (s->stack_top != stack_top) {
    fprintf(stderr, "Internal error: Stack top not restored after statement list finalization, expected %" PRIuMAX " received %" PRIuMAX ". This should never happen!\n", stack_top, s->stack_top);
    exit(1);
  }

  s->reg_num = reg_num; /* Restore our register count */
}

static void process_dbnz_stmt(struct dbnz_compile_state *s, struct dbnz_statement *stmts, struct deferred *deferred) {
  if (s->exec_num + 2 > s->exec_max) {
      s->execs = realloc(s->execs, sizeof(struct dbnz_rval *) * (s->exec_max += 250));
  }
  s->execs[s->exec_num] = process_rval(s, deferred, stmts->u.d.target);
  ++s->exec_num;
  s->execs[s->exec_num] = process_rval(s, deferred, stmts->u.d.jump);
  ++s->exec_num;
}

static void process_stmts(struct dbnz_compile_state *s, struct dbnz_statementlist *stmtlist, struct dbnz_macro *macros, struct locals locals) {
#ifdef DBNZ_CELLNO_TRACE
  size_t start = s->exec_num;
#endif
  /* If we're the top level, our local labels are globals */
  if (!s->globals) {
    s->globals = &locals;
  } else {
    /* Safety checks, to protect against recusive inlining. */
    if (s->visited_num == s->visited_max) {
      s->visited = realloc(s->visited, sizeof(char *) * (s->visited_max += 10));
    }
    s->visited[s->visited_num++] = s->context;
  }
  struct dbnz_statement *stmts = stmtlist->list;
  struct deferred deferred;
  memset(&deferred, '\0', sizeof(struct deferred));
  /* Our statement list must have at least one statement to be valid */
  do {
    switch (stmts->type) {
    case STMT_LABEL:
      process_label_stmt(s, stmts, &locals);
      break;
    case STMT_CALL:
      process_call_stmt(s, stmts, &deferred, macros);
      break;
    case STMT_DBNZ:
      process_dbnz_stmt(s, stmts, &deferred);
      break;
    }
    stmts = stmts->next;
  } while (stmts);
  --s->visited_num;
#ifdef DBNZ_CELLNO_TRACE
  printf("Context %s from cells (prog + %" PRIuMAX ") to (prog + %" PRIuMAX ").\n", context, (uintmax_t) start, (uintmax_t) s->exec_num);
#endif
  finalize_deferred(s, &deferred, &locals);
}

struct dbnz_compile_state *dbnz_compile_program(struct dbnz_statementlist *stmtlist, struct dbnz_macro *macros, size_t memory) {
  struct locals l;
  memset(&l, '\0', sizeof(struct locals));
  struct dbnz_compile_state *s = malloc(sizeof(struct dbnz_compile_state));
  memset(s, '\0', sizeof(struct dbnz_compile_state));
  s->proglen = memory;
  s->context = "<top level>";
  process_stmts(s, stmtlist, macros, l);
  return s;
}
