%{
#ifdef ENABLE_TRACE
#define YYDEBUG 1
#endif
    #include <stdlib.h>
    #include <string.h>
    #include "rdbnz.h"

    #include "y.tab.h"

    #include "reentrant_decl_fix.h"

    #include "lex.yy.h"

    void push_input_file(const char *name, yyscan_t scanner);

    static void yyerror(yyscan_t scanner, struct dbnz_macro **macrolist, struct dbnz_statementlist *stmtlist, const char *s) {
      fprintf(stderr, "Error '%s' at line %d, near %s:\n > %s\n", s, yyget_lineno(scanner), yyget_text(scanner), yyget_extra(scanner));
    }

    static unsigned current_file_id(yyscan_t scanner) {
      struct dbnz_parse_state *parse_state = yyget_extra(scanner);
      return parse_state->source_file_num;
    }
%}

%define api.pure full
%param {void *scanner} {struct dbnz_macro **macrolist} {struct dbnz_statementlist *stmtlist}

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

%token DEF THIS DBNZ DATA BLANKLINE IMPORT
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
        chomp importlist macrolist statementlist chomp {
                                                *macrolist = $3;
                                                *stmtlist = $4;
                                              }
        ;

importlist:
        import chomp importlist
        | /* NULL */
        ;

import:
        IMPORT IDENTIFIER                     { push_input_file($2, scanner); }

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
        LABEL                                 { $$ = malloc(sizeof(struct dbnz_statement)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = STMT_LABEL; $$->u.n = $1; }
        | IDENTIFIER '(' rvallist ')'         { $$ = malloc(sizeof(struct dbnz_statement)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = STMT_CALL; $$->u.c = (struct dbnz_call_data) { .target = $1, .argv = $3 }; }
        | DBNZ rval ',' rval                  { $$ = malloc(sizeof(struct dbnz_statement)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = STMT_DBNZ; $$->u.d = (struct dbnz_dbnz_data) { .target = $2, .jump = $4 }; }
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
        INTEGER                               { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = RVAL_INTEGER; $$->u.v = $1; }
        | STACK                               { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = RVAL_STACK; $$->u.v = $1; } /* We have to wait until invocation before we can assign stack cells; this is done on the second scan */
        | CONSTANT                            { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = RVAL_CONSTANT; $$->u.v = $1; }
        | DATA                                { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = RVAL_DATA; } /* We need to have the constant pool and program finalized before we know this */
        | THIS                                { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = RVAL_THIS; } /* We need to have the constant pool finalized before we know this */
        | IDENTIFIER                          { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = RVAL_IDENTIFIER; $$->u.n = $1; } /* We don't know this due to initialization order, handle in 2nd pass */
        | rval '+' rval                       { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = RVAL_ADD; $$->u.p = (struct dbnz_expr) { .lhs = $1, .rhs = $3 }; }
        | rval '-' rval                       { $$ = malloc(sizeof(struct dbnz_rval)); $$->line_no = yyget_lineno(scanner); $$->file_no = current_file_id(scanner); $$->type = RVAL_SUB; $$->u.p = (struct dbnz_expr) { .lhs = $1, .rhs = $3 }; }
        ;

chomp:
        BLANKLINE chomp
        | /* NULL */
        ;

%%
