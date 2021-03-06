
#ifndef RDBNZ_H
#define RDBNZ_H

#include <stdlib.h>

/* The maximum line length we store for parse and compile error reporting */
/* This has no effect on lexing+parsing itself, which is handled by flex+bison */
#define LINEBUF_LEN 500

/* Allow imports up to 20 deep */
#define IMPORT_MAX 20

#define QUAL_DESTRUCTIVE 1

struct dbnz_rval;
struct dbnz_expr {
  struct dbnz_rval *lhs;
  struct dbnz_rval *rhs;
};
struct dbnz_rval {
  enum {
    RVAL_INTEGER,
    RVAL_STACK,
    RVAL_CONSTANT, /* Currently unused, we resolve this to an integer at parse time */
    RVAL_THIS,
    RVAL_DATA,
    RVAL_ADD,
    RVAL_SUB,
    /*
     * The identifier system is probably the most complicated mechanism in an
     * otherwise very straightforward compiler.
     *
     * Since we specifically permit shadowing within a context, we must defer
     * identifier resolution until we've finished processing our statement list.
     *
     * For example, we may have a statement in a macro that should resolve to a
     * label :end later in the same macro, which shadows a global label :end.
     *
     * We cannot know what we haven't seen yet, so this needs to be processed
     * at the end.
     *
     * This is further compounded by permitting references to identifiers that
     * cannot be resolved yet, ie. arguments, since they'll only be resolvable
     * one the enclosing statement list is finalized.
     *
     * In order to keep this all transparent to the user, we have three types
     * of identifier rval here:
     *  - RVAL_IDENTIFIER is used during parsing, and should never be modified.
     *    This is needed as a blueprint for any later macro calls.
     *  - RVAL_IDENTCOPY is a copy of RVAL_IDENTIFIER, and is the authorative
     *    allocation for resolution where it appears. Only one RVAL_IDENTCOPY
     *    should be made, even if it is passed as an identifier to a nested
     *    macro.
     *  - Here, RVAL_REF is a thin reference that must point to RVAL_IDENTCOPY.
     *    Usable in a nested context, this is only guarunteed to hold a
     *    meaningful value once the top level context has been finalized.
     *    Stronger assertions (once its enclosing context is finalized) may be
     *    possible but are not required.
     */
    RVAL_IDENTIFIER,
    RVAL_IDENTCOPY,
    RVAL_REF,
    /*
     * Label is a special type marked at runtime to denote labels.
     * Its "real" value is its stored value plus the program offset, ie. the
     * size of the constant pool.
     */
    RVAL_LABEL,
  } type;
  union {
    size_t v;
    struct dbnz_expr p;
    char *n;
    struct dbnz_rval *r;
  } u;
  int quals;
  size_t line_no;
  unsigned file_no;
  struct dbnz_rval *next;
};

struct dbnz_call_data {
  char *target;
  struct dbnz_rval *argv;
};

struct dbnz_dbnz_data {
  struct dbnz_rval *target;
  struct dbnz_rval *jump;
};

struct dbnz_statement {
  enum {
    STMT_LABEL,
    STMT_CALL,
    STMT_DBNZ
  } type;
  union {
    char *n;
    struct dbnz_call_data c;
    struct dbnz_dbnz_data d;
  } u;
  size_t line_no;
  unsigned file_no;
  struct dbnz_statement *next;
};

struct dbnz_statementlist {
  struct dbnz_statement *list;
  size_t stack_needed;
};

struct dbnz_param {
  char *id;
  int quals;
  struct dbnz_param *next;
};

struct dbnz_macro {
  char *id;
  struct dbnz_param *argv;
  struct dbnz_statementlist statements;
  struct dbnz_macro *next;
};

struct dbnz_source {
  char *name;
  unsigned source_line_num;
  unsigned source_line_max;
  char **source_lines;
};

struct dbnz_parse_state {
  char *linebuf;
  int source_file_num;
  int source_file_max;
  struct dbnz_source *source_files;
  unsigned import_depth;
  unsigned imports[IMPORT_MAX];
};

struct locals;
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
  /* Name of the innermost context currently being processed. */
  const char *context;
};


#endif /* RDBNZ_H */
