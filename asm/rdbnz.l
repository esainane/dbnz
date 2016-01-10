%{
#include <stdlib.h>
#include <string.h>
#include "rdbnz.h"
#include "y.tab.h"
void yyerror(char *);

static char linebuf[500];
int source_line_num = 0;
static int source_line_max = 0;
char **source_lines;

const unsigned buf_size = YY_BUF_SIZE;

void yyerror(char *s) {
    fprintf(stderr, "Error '%s' at line %d, near %s:\n > %s\n", s, yylineno, yytext, linebuf);
}

#define INPUT_NEWLINE(never_blank) { \
  if (yytext[1]) { \
    /* Not a blank line */ \
    strncpy(linebuf, yytext+1, sizeof(linebuf)); \
    if (yylineno >= source_line_max) { \
      source_lines = realloc(source_lines, sizeof(char *) * (source_line_max += 50)); \
    } \
    if (++source_line_num > yylineno) { \
      memset(source_lines + source_line_num, '\0', yylineno - source_line_num); \
      source_line_num = yylineno; \
    } \
    source_lines[yylineno] = strdup(yytext+1); \
    yyless(1); \
  } else if (!never_blank) { \
    /* Blank line, ie. \n\n */ \
    return BLANKLINE; \
  } \
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
                INPUT_NEWLINE(0);
                yy_pop_state();
            }
}

<MULTILINE_COMMENT>{
"*/"        yy_pop_state();
[^*\n]+
\n[^\n]*    {
                INPUT_NEWLINE(1);
            }
}

<MULTILINE_COMMENT><<EOF>> {
                yyerror("Reached end of file in an unterminated multiline comment!");
                yyterminate();
            }

  /* Main rules */

<<EOF>> {
  yypop_buffer_state();
  if (!YY_CURRENT_BUFFER) {
    yyterminate();
  }
}

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
                  INPUT_NEWLINE(0);
            }

[-+(),]     {
                return *yytext;
            }

"def"           return DEF;
"this"          return THIS;
"data"          return DATA;
"dbnz"          return DBNZ;
"import"        return IMPORT;

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
