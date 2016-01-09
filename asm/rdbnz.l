%{
#include <stdlib.h>
#include "rdbnz.h"
#include "y.tab.h"
void yyerror(char *);

char linebuf[500];
int dirty = 0;

void yyerror(char *s) {
    fprintf(stderr, "Error '%s' at line %d, near %s:\n > %s\n", s, yylineno, yytext, linebuf);
}
%}

%option stack
%option yylineno

%x COMMENT
%x MULTILINE_COMMENT

%%

  /* Comment state switching rules */

<INITIAL>{
";"         yy_push_state(COMMENT);
"//"        yy_push_state(COMMENT);
}

<INITIAL>{
"/*"        yy_push_state(MULTILINE_COMMENT);
}

  /* Comments eat everything until they respectively reach their \n or closing */

<COMMENT>{
[^\n]+
\n[^\n]*    {
                strncpy(linebuf, yytext+1, sizeof(linebuf));
                yyless(1);
                yy_pop_state();
            }
}

<MULTILINE_COMMENT>{
"*/"        yy_pop_state();
[^*\n]+
\n[^\n]*    {
                strncpy(linebuf, yytext+1, sizeof(linebuf));
                yyless(1);
            }
}

<MULTILINE_COMMENT><<EOF>> {
                yyerror("Reached end of file in an unterminated multiline comment!");
                yyterminate();
            }

  /* Main rules */

0           {
                yylval.i = atoi(yytext);
                return INTEGER;
            }

[1-9][0-9]* {
                yylval.i = atoi(yytext);
                return INTEGER;
            }

&0          {
                yylval.i = atoi(yytext + 1);
                return CONSTANT;
            }

&[1-9][0-9]* {
                yylval.i = atoi(yytext + 1);
                return CONSTANT;
            }

@[1-9][0-9]* {
                yylval.i = atoi(yytext + 1);
                return STACK;
            }

\n[^\n]*    {
                if (yytext[1]) {
                  /* Not a blank line */
                  strncpy(linebuf, yytext+1, sizeof(linebuf));
                  printf("line: %s\n", yytext+1);
                  yyless(1);
                } else {
                  /* Blank line, ie. \n\n */
                  return BLANKLINE;
                }
            }

[-+(),]     {
                return *yytext;
            }

"def"           return DEF;
"this"          return THIS;
"data"          return DATA;
"dbnz"          return DBNZ;

[ \t]+      ;   /* ignore whitespace */

:[a-z]+     {
                yylval.s = strdup(yytext + 1);
                return LABEL;
            }

[a-z]+[a-z0-9]* {
                yylval.s = strdup(yytext);
                return IDENTIFIER;
            }

.           yyerror("Unknown character");


%%
int yywrap(void) {
    return 1;
}
