
#include "rdbnz.h"
#include "rdbnz_pool.h"
#include "rdbnz_write.h"

#include <string.h>
#include <errno.h>
#include <inttypes.h>

extern char **source_lines;

static const char out_padding[] = "                        ";

#define ALIGN(x) ((x + 1) & ~1)

static size_t check_written_or_die(int val) {
  int err = errno;
  if (val < 0) {
    fprintf(stderr, "Failed to write output data, error %s (%d), aborting!", strerror(err), err);
    exit(1);
  }
  return val;
}

static void write_constants(FILE * output, struct dbnz_compile_state *s) {
  size_t i;
  for (i = 0; i != s->constant_num; ++i) {
    size_t len = check_written_or_die(fprintf(output, "%" PRIuMAX, (uintmax_t) s->constants[i]));
    fprintf(output, "%s; cell %3" PRIuMAX "    ; constant %" PRIuMAX "\n", len < sizeof(out_padding) ? out_padding + len : "", (uintmax_t) i, (uintmax_t) s->constants[i]);
  }
  if (s->constant_num & 1) {
    size_t len = check_written_or_die(fprintf(output, "0"));
    fprintf(output, "%s; cell %3" PRIuMAX "    ; alignment padding\n", len < sizeof(out_padding) ? out_padding + len : "", (uintmax_t) s->constant_num);
    ++s->constant_num;
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
    return ALIGN(s->constant_num) + s->exec_num;
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
    return rval->u.v + ALIGN(s->constant_num);
  case RVAL_INTEGER:
    /* Nothing to do */
    return rval->u.v;
  }
}

/* Write the program to state output, fully resolving all expressions */
static void write_program(FILE * output, struct dbnz_compile_state *s, const struct dbnz_source *source_files, size_t source_file_num) {
  size_t i;
  size_t len = 0;
  for (i = 0; i != s->exec_num; ++i) {
    size_t val = check_written_or_die(resolve_rval(s, s->execs[i], i + ALIGN(s->constant_num)));
    len += fprintf(output, "%" PRIuMAX " ", (uintmax_t) val);
    if (i & 1) {
      if (s->execs[i]->file_no > source_file_num) {
        fprintf(stderr, "Internal error: Cell refers to file %" PRIuMAX " at cell %" PRIuMAX " when %" PRIuMAX " files have been parsed, this should never happen!\n", (uintmax_t) s->execs[i]->file_no, (uintmax_t) i, (uintmax_t) source_file_num);
        exit(1);
      }
      const struct dbnz_source *file = source_files + s->execs[i]->file_no;
      if (s->execs[i]->line_no > file->source_line_num) {
        fprintf(stderr, "Internal error: Marked line number %" PRIuMAX " at cell %" PRIuMAX " exceeds maximum line number %" PRIuMAX " for file %u, this should never happen!\n", (uintmax_t) s->execs[i]->line_no, (uintmax_t) i, (uintmax_t) file->source_line_num, s->execs[i]->file_no);
        exit(1);
      }
      fprintf(output, "%s; cell %3" PRIuMAX ",%3" PRIuMAX "; source line %3" PRIuMAX ": %s\n",
              len < sizeof(out_padding) ? out_padding + len : "",
              s->constant_num + i - 1,
              s->constant_num + i,
              s->execs[i]->line_no,
              file->source_lines[s->execs[i]->line_no] ? file->source_lines[s->execs[i]->line_no] : "<NULL>");
      len = 0;
    }
  }
}

void dbnz_write_all(FILE * output, struct dbnz_compile_state *s, const struct dbnz_source *source_files, size_t source_file_num) {
  fprintf(output, "%" PRIuMAX " %" PRIuMAX "\n", (uintmax_t) s->proglen, (uintmax_t) ALIGN(s->constant_num));
  write_constants(output, s);
  write_program(output, s, source_files, source_file_num);
}
