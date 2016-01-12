%{
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rdbnz.h"

#include "y.tab.h"

#include "reentrant_decl_fix.h"

/* Compile error reporting */

#define lexerror(s) fprintf(stderr, "Error '%s' at line %d, near %s:\n > %s\n", s, yylineno, yytext, yyextra)

int input_newline(yyscan_t scanner);

%}

%option reentrant bison-bridge

%option stack
%option yylineno

%x COMMENT
%x MULTILINE_COMMENT

%%

  /* Comment state switching rules */

<INITIAL>{
";"         yy_push_state(COMMENT, yyscanner);
"//"        yy_push_state(COMMENT, yyscanner);
}

<INITIAL>{
"/*"        yy_push_state(MULTILINE_COMMENT, yyscanner);
}

  /* Comments eat everything until they respectively reach their \n or closing */

<COMMENT>{
[^\n]+
\n[^\n]*    {
                if (input_newline(yyscanner)) {
                  yyless(1);
                } else {
                  return BLANKLINE;
                }
                yy_pop_state(yyscanner);
            }
}

<MULTILINE_COMMENT>{
"*/"        yy_pop_state(yyscanner);
[^*\n]+
\n[^\n]*    {
                if (input_newline(yyscanner)) {
                  yyless(1);
                }
            }
}

<MULTILINE_COMMENT><<EOF>> {
                lexerror("Reached end of file in an unterminated multiline comment!");
                yyterminate();
            }

  /* Main rules */

<<EOF>> {
  yypop_buffer_state(yyscanner);
  if (!YY_CURRENT_BUFFER) {
    yyterminate();
  } else {
    struct dbnz_parse_state *parse_state = yyextra;
    --parse_state->import_depth;
  }
}

0           {
                yylval->i = atoi(yytext);
                return INTEGER;
            }

[1-9][0-9]* {
                yylval->i = atoi(yytext);
                return INTEGER;
            }

&0          {
                yylval->i = atoi(yytext + 1);
                return CONSTANT;
            }

&[1-9][0-9]* {
                yylval->i = atoi(yytext + 1);
                return CONSTANT;
            }

@[1-9][0-9]* {
                yylval->i = atoi(yytext + 1);
                return STACK;
            }

\n[^\n]*    {
                if (input_newline(yyscanner)) {
                  yyless(1);
                } else {
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
"import"        return IMPORT;

[ \t]+      ;   /* ignore whitespace */

:[a-z]+     {
                yylval->s = strdup(yytext + 1);
                return LABEL;
            }

[a-z]+[a-z0-9]* {
                yylval->s = strdup(yytext);
                return IDENTIFIER;
            }

!           ;   /* Ignore */

.           {
                lexerror("Unknown character");
                yyterminate();
            }

%%

void push_input_file(const char *name, yyscan_t scanner) {
  char *filename = malloc(strlen(name) + 6 + 1);
  sprintf(filename, "%s.rdbnz", name);
  struct dbnz_parse_state *parse_state = yyget_extra(scanner);

  /* Save the index of our current file */
  if (++parse_state->import_depth == IMPORT_MAX) {
    fprintf(stderr, "Too many nested imports (%d), aborting!\n", IMPORT_MAX);
    exit(1);
  }
  FILE * in = fopen(filename, "r");
  if (!in) {
    fprintf(stderr, "Unable to open target import file (%s), aborting!\n", filename);
    exit(1);
  }

  /* import depth raises and lowers, but source_file_num is always monotonically increasing. */
  if (++parse_state->source_file_num >= parse_state->source_file_max) {
    parse_state->source_files = realloc(parse_state->source_files, sizeof(struct dbnz_source) * (parse_state->source_file_max += 5));
    memset(parse_state->source_files + parse_state->source_file_max - 5, '\0', sizeof(struct dbnz_source) * 5);
  }
  ++parse_state->source_file_num;
  parse_state->imports[parse_state->import_depth] = parse_state->source_file_num;
  //parse_state->source_files[parse_state->source_file_num].name = filename;

  /* Finally, set up our new input. */
  yyset_in(in, scanner);
  yypush_buffer_state(yy_create_buffer(in, YY_BUF_SIZE, scanner), scanner);
}

int input_newline(yyscan_t scanner) {
  if (yyget_text(scanner)[1]) {
    /* Not a blank line */
    struct dbnz_parse_state *parse_state = yyget_extra(scanner);
    /* If we don't yet have data for storing files, create one now. */
    if (!parse_state->source_file_max) {
      parse_state->source_files = malloc(sizeof(struct dbnz_source) * (parse_state->source_file_max += 1));
      memset(parse_state->source_files, '\0', sizeof(struct dbnz_source));
    }
    struct dbnz_source *current_file_source = parse_state->source_files + parse_state->imports[parse_state->import_depth];
    strncpy(parse_state->linebuf, yyget_text(scanner) + 1, LINEBUF_LEN - 1);
    /* If storage for tracking our current file isn't enough, grow it. */
    while (yyget_lineno(scanner) >= current_file_source->source_line_max) {
      current_file_source->source_lines = realloc(current_file_source->source_lines, sizeof(char *) * (current_file_source->source_line_max += 50));
      memset(current_file_source->source_lines + current_file_source->source_line_max - 50, '\0', sizeof(char *) * 50);
    }
    current_file_source->source_line_num = yyget_lineno(scanner);
    /* Add our source line. */
    current_file_source->source_lines[yyget_lineno(scanner)] = strdup(parse_state->linebuf);
    return 1;
  }
  /* Blank line, ie.n\n */
  return 0;
}

int yywrap(yyscan_t yyscanner) {
  return 1;
}
