%{
#ifdef ENABLE_TRACE
#define YYDEBUG 1
#endif
    #include <stdlib.h>
    #include <stdio.h>
    #include "rdbnz.h"
    #include "rdbnz_compile.h"
    void yyerror(char *);
    int yylex(void);
    extern int yylineno;
    extern FILE * yyin;
    FILE * dbnz_output;

    static size_t dbnz_mem;
    static struct dbnz_compile_state dbnz_state;
%}

%union {
    int i;
    char *s;
    struct dbnz_rval *rval;
    struct dbnz_param *param;
    struct dbnz_statement *stmt;
    struct dbnz_statementlist stmtlist;
    struct dbnz_macro *macro;
};

/* Tokens */

%token DEF THIS DBNZ DATA BLANKLINE
%token <s> LABEL IDENTIFIER
%token <i> INTEGER CONSTANT STACK
%left '+' '-'

%type <rval> rval
%type <rval> rvallist
%type <rval> rvallisttail
%type <param> paramlist
%type <param> paramlisttail
%type <stmt> statement
%type <stmtlist> statementlist
%type <stmt> statementlisttail
%type <macro> macro
%type <macro> macrolist

/* Debugging */

%printer { fprintf (yyoutput, "<label '%s'>", $$); } LABEL
%printer { fprintf (yyoutput, "<identifier '%s'>", $$); } IDENTIFIER
%printer { fprintf (yyoutput, "<integer '%d'>", $$); } INTEGER


/* Main rules */

%%

program:
        chomp macrolist statementlist chomp   { dbnz_compile_program($2, $3, dbnz_mem, &dbnz_state); }
        ;

macrolist:
        macro chomp macrolist                 { $$ = $1; $$->next = $3; }
        | /* NULL */                          { $$ = 0; }
        ;

macro:
        DEF IDENTIFIER '(' paramlist ')'
        statementlist BLANKLINE               { $$ = malloc(sizeof(struct dbnz_macro)); $$->id = $2; $$->argv = $4; $$->statements = $6; }
        ;

paramlist:
        IDENTIFIER paramlisttail              { $$ = malloc(sizeof(struct dbnz_param)); $$->id = $1; $$->next = $2; }
        | /* NULL */                          { $$ = 0; }
        ;

paramlisttail:
        ',' IDENTIFIER paramlisttail          { $$ = malloc(sizeof(struct dbnz_param)); $$->id = $2; $$->next = $3; }
        | /* NULL */                          { $$ = 0; }
        ;

statementlist:
        statement statementlisttail           { $1->next = $2; $$ = (struct dbnz_statementlist) { .list = $1 }; }
        ;

statementlisttail:
        statement statementlisttail           { $$ = $1; $$->next = $2; }
        | /* NULL */                          { $$ = 0; }
        ;

statement:
        LABEL                                 { $$ = malloc(sizeof(struct dbnz_statement)); $$->line_no = yylineno; $$->type = STMT_LABEL; $$->u.n = $1; }
        | IDENTIFIER '(' rvallist ')'         { $$ = malloc(sizeof(struct dbnz_statement)); $$->line_no = yylineno; $$->type = STMT_CALL; $$->u.c = (struct dbnz_call_data) { .target = $1, .argv = $3 }; }
        | DBNZ rval ',' rval                  { $$ = malloc(sizeof(struct dbnz_statement)); $$->line_no = yylineno; $$->type = STMT_DBNZ; $$->u.d = (struct dbnz_dbnz_data) { .target = $2, .jump = $4 }; }
        ;

rvallist:
        rval rvallisttail                     { $$ = $1; $$->next = $2; }
        | /* NULL */                          { $$ = 0; }
        ;

rvallisttail:
        ',' rval rvallisttail                 { $$ = $2; $$->next = $3; }
        | /* NULL */                          { $$ = 0; }
        ;

rval:
        INTEGER                               { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yylineno; $$->type = RVAL_INTEGER; $$->u.v = $1; }
        | STACK                               { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yylineno; $$->type = RVAL_STACK; $$->u.v = $1; } /* We have to wait until invocation before we can assign stack cells; this is done on the second scan */
        | CONSTANT                            { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yylineno; $$->type = RVAL_INTEGER; $$->u.v = dbnz_pool_constant($1, &dbnz_state); }
        | DATA                                { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yylineno; $$->type = RVAL_DATA; } /* We need to have the constant pool and program finalized before we know this */
        | THIS                                { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yylineno; $$->type = RVAL_THIS; } /* We need to have the constant pool finalized before we know this */
        | IDENTIFIER                          { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yylineno; $$->type = RVAL_IDENTIFIER; $$->u.n = $1; } /* We don't know this due to initialization order, handle in 2nd pass */
        | rval '+' rval                       { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yylineno; $$->type = RVAL_ADD; $$->u.p = (struct dbnz_expr) { .lhs = $1, .rhs = $3 }; }
        | rval '-' rval                       { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yylineno; $$->type = RVAL_SUB; $$->u.p = (struct dbnz_expr) { .lhs = $1, .rhs = $3 }; }
        ;

chomp:
        BLANKLINE chomp
        | /* NULL */
        ;

%%

/* Compilation */

static void usage(void) {
  fprintf(stderr, "Please specify the amount of memory the machine should have, and optionally the input and output files.\n");
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "./dbnzc <cells> [infile] [outfile]\n");
  exit(1);
}

int main(int argc, char **argv) {
  dbnz_output = stdout;
  switch (argc) {
  case 4:
    dbnz_output = fopen(argv[3], "w");
    if (!dbnz_output) {
      fprintf(stderr, "Could not open output (%s) for writing!\n", argv[3]);
      usage();
    }
    /* Fall through */
  case 3:
    yyin = fopen(argv[2], "r");
    if (!yyin) {
      fprintf(stderr, "Could not open input (%s) for writing!\n", argv[2]);
      usage();
    }
    /* Fall through */
  case 2:
    dbnz_mem = atoi(argv[1]);
    if (!dbnz_mem) {
      fprintf(stderr, "Machine requires memory to run, '%s is not a valid memory value!\n", argv[1]);
      usage();
    }
    break;
  default:
    usage();
  }
#ifdef ENABLE_TRACE
  yydebug = 1;
#endif
  yyparse();
  /* XXX: Assumes all memory is cleared on SIGSEG... er, I mean program exit. :) */
  return 0;
}
