
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
  struct dbnz_rval **idents;
};

extern FILE * dbnz_output;
extern char **source_lines;

static unsigned int rval_copy_count = 0;
static void process_deferred(struct dbnz_compile_state *s, struct deferred *deferred, struct locals *locals);
static struct dbnz_rval *process_rval(struct dbnz_compile_state *s, struct deferred *deferred, struct dbnz_rval *rval);

static const char out_padding[] = "                        ";

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
    fprintf(stderr, "Macro '%s' called from %s requires %d more arguments (expected %d)!\n", target_id, context, needed, locals->num + needed);
    exit(1);
  }
  if (args) {
    int extra = 0;
    while (++extra, args = args->next) ;
    fprintf(stderr, "Macro '%s' called from %s requires %d fewer arguments (expected %d)!\n", target_id, context, extra, locals->num);
    exit(1);
  }
}

/* XXX: God function. Split me. */
static void process_stmts(struct dbnz_macro *macros, struct dbnz_statementlist *stmtlist, struct dbnz_compile_state *s, struct locals locals, const char *context) {
  size_t start = s->exec_num;
  /* If we're the top level, our local labels are globals */
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
      *locals.vals[locals.num] = (struct dbnz_rval) { .line_no = stmts->line_no, .type = RVAL_LABEL, .u = { .v = s->exec_num }};
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
      add_locals(s, target->argv, stmts->u.c.argv, &target_locals, &deferred, target_id, context);
      size_t stack_top = s->stack_top; /* Sanity check */
      size_t reg_num = s->reg_num; /* Save the number of registers we are using. */
      s->stack_top += reg_num;
      process_stmts(macros, &target->statements, s, target_locals, target_id);
      s->stack_top -= reg_num;
      /* Perform our sanity check */
      if (s->stack_top != stack_top) {
        fprintf(stderr, "Internal error: Stack top not restored after statement list finalization, expected %" PRIuMAX " received %" PRIuMAX ". This should never happen!\n", stack_top, s->stack_top);
        exit(1);
      }
      s->reg_num = reg_num; /* Restore our register count */
    } break;
    case STMT_DBNZ:
      if (s->exec_num + 2 > s->exec_max) {
          s->execs = realloc(s->execs, sizeof(struct dbnz_rval *) * (s->exec_max += 250));
      }
      s->execs[s->exec_num] = process_rval(s, &deferred, stmts->u.d.target);
      ++s->exec_num;
      s->execs[s->exec_num] = process_rval(s, &deferred, stmts->u.d.jump);
      ++s->exec_num;
      break;
    }
    stmts = stmts->next;
  } while (stmts);
  --s->visited_num;
#ifdef DBNZ_CELLNO_TRACE
  printf("Context %s from cells (prog + %" PRIuMAX ") to (prog + %" PRIuMAX ").\n", context, (uintmax_t) start, (uintmax_t) s->exec_num);
#endif
  process_deferred(s, &deferred, &locals);
}

static void write_constants(struct dbnz_compile_state *s) {
  size_t i;
  for (i = 0; i != s->constant_num; ++i) {
    int len = fprintf(dbnz_output, "%" PRIuMAX, (uintmax_t) s->constants[i]);
    fprintf(dbnz_output, "%s; cell %3" PRIuMAX "    ; constant %" PRIuMAX "\n", len < sizeof(out_padding) ? out_padding + len : "", (uintmax_t) i, (uintmax_t) s->constants[i]);
  }
  if (s->constant_num & 1) {
    int len = fprintf(dbnz_output, "0");
    fprintf(dbnz_output, "%s; cell %3" PRIuMAX "    ; alignment padding\n", len < sizeof(out_padding) ? out_padding + len : "", (uintmax_t) s->constant_num);
    ++s->constant_num;
  }
}

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
    *val = (struct dbnz_rval) { .type = RVAL_INTEGER, .line_no = rval->line_no, .u = { .v = s->proglen - (s->stack_top + rval->u.v) }};
    break;
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
    ref->type = RVAL_REF;
    ref->u.r = cpy;
    ref->line_no = cpy->line_no;
    /* We'll later modify this copy, resolving it based on local context. */
    deferred->idents[deferred->num++] = cpy;
    return ref;
  case RVAL_IDENTCOPY:
    fprintf(stderr, "Tried to process RVAL_IDENTCOPY not guarded by RVAL_REF, this should never happen!\n");
    exit(1);
  case RVAL_REF:
    /* Deliberately do nothing, this will already be handled */
  case RVAL_CONSTANT:
  case RVAL_THIS:
  case RVAL_DATA:
  case RVAL_INTEGER:
  case RVAL_LABEL:
    /* Nothing to do */
    return rval;
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

static void process_deferred(struct dbnz_compile_state *s, struct deferred *deferred, struct locals *locals) {
  int i;
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

/* Fully resolve an rval */
static size_t resolve_rval(struct dbnz_compile_state *s, struct dbnz_rval *rval, size_t where) {
  switch (rval->type) {
  case RVAL_CONSTANT:
    fprintf(stderr, "Internal error: Found unresolved constant in rval during final compilation pass, this should never happen!\n");
    exit(1);
  case RVAL_STACK:
    fprintf(stderr, "Internal error: Found unresolved stack address in rval during final compilation pass, this should never happen!\n");
    exit(1);
  case RVAL_DATA:
    return s->constant_num + s->exec_num;
  case RVAL_REF:
    return resolve_rval(s, rval->u.r, where);
  case RVAL_IDENTCOPY:
  case RVAL_IDENTIFIER:
    fprintf(stderr, "Internal error: Found unresolved %sidentifier '%s' in rval at cell '%" PRIuMAX "' (prog + '%" PRIuMAX "') during final compilation pass, this should never happen!\n",
            rval->type == RVAL_IDENTCOPY ? "copy of " : "", rval->u.n, (uintmax_t) where, (uintmax_t) (where - s->constant_num));
    exit(1);
  case RVAL_ADD:
    return resolve_rval(s, rval->u.p.lhs, where) + resolve_rval(s, rval->u.p.rhs, where);
  case RVAL_SUB:
    return resolve_rval(s, rval->u.p.lhs, where) - resolve_rval(s, rval->u.p.rhs, where);
  case RVAL_THIS:
    return where;
  case RVAL_LABEL:
    return rval->u.v + s->constant_num;
  case RVAL_INTEGER:
    /* Nothing to do */
    return rval->u.v;
  }
}

/* Write the program to state output, fully resolving all expressions */
static void write_program(struct dbnz_compile_state *s) {
  size_t i;
  int len = 0;
  for (i = 0; i != s->exec_num; ++i) {
    size_t val = resolve_rval(s, s->execs[i], i + s->constant_num);
    len += fprintf(dbnz_output, "%" PRIuMAX " ", (uintmax_t) val);
    if (i & 1) {
      fprintf(dbnz_output, "%s; cell %3d,%3d; source line %3d: %s\n", len < sizeof(out_padding) ? out_padding + len : "", s->constant_num + i - 1, s->constant_num + i, s->execs[i]->line_no, source_lines[s->execs[i]->line_no] ? source_lines[s->execs[i]->line_no] : "<NULL>");
      len = 0;
    }
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
  s->proglen = memory;
  process_stmts(macros, &stmtlist, s, l, "<top level>");

  fprintf(dbnz_output, "%" PRIuMAX " %" PRIuMAX "\n", (uintmax_t) memory, (uintmax_t) (s->constant_num & 1 ? s->constant_num + 1 : s->constant_num));
  write_constants(s);
  write_program(s);
}
