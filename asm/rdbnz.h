
#ifndef RDBNZ_H
#define RDBNZ_H

struct dbnz_rval;
struct dbnz_expr {
  struct dbnz_rval *lhs;
  struct dbnz_rval *rhs;
};
struct dbnz_rval {
  enum {
    RVAL_INTEGER,
    RVAL_STACK,
    RVAL_CONSTANT,
    RVAL_THIS,
    RVAL_DATA,
    RVAL_IDENTIFIER,
    RVAL_ADD,
    RVAL_SUB,
    RVAL_LABEL
  } type;
  union {
    size_t v;
    struct dbnz_expr p;
    char *n;
  } u;
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
  struct dbnz_statement *next;
};

struct dbnz_statementlist {
  struct dbnz_statement *list;
  size_t stack_needed;
};

struct dbnz_param {
  char *id;
  struct dbnz_param *next;
};

struct dbnz_macro {
  char *id;
  struct dbnz_param *argv;
  struct dbnz_statementlist statements;
  struct dbnz_macro *next;
};

#endif /* RDBNZ_H */
