
#ifndef REENTRANT_DECL_FIX
#define REENTRANT_DECL_FIX

/*
 * The %lex-params information should be passed to the generated files via headers. Unfortunately, it currently does not.
 *
 * This file is here to provide a workaround.
 */
int yylex(YYSTYPE * yylval_param, void * yyscanner, struct dbnz_macro **macrolist, struct dbnz_statementlist *stmtlist);


#define YY_DECL int yylex \
               (YYSTYPE * yylval_param, yyscan_t yyscanner, struct dbnz_macro **macrolist, struct dbnz_statementlist *stmtlist)

#endif /* REENTRANT_DECL_FIX */
