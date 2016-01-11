
#include "rdbnz_compile.h"
#include "rdbnz_pool.h"
#include "rdbnz_write.h"

#include "y.tab.h"
#include "lex.yy.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* Compilation */

static void usage(void) {
  fprintf(stderr, "Please specify the amount of memory the machine should have, and optionally the input and output files.\n");
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "./dbnzc <cells> [infile] [outfile]\n");
  exit(1);
}

int main(int argc, char **argv) {
  FILE * output = stdout;
  FILE * input = stdin;
  size_t memory;
  switch (argc) {
  case 4:
    output = fopen(argv[3], "w");
    if (!output) {
      fprintf(stderr, "Could not open output (%s) for writing!\n", argv[3]);
      usage();
    }
    /* Fall through */
  case 3:
    input = fopen(argv[2], "r");
    if (!input) {
      fprintf(stderr, "Could not open input (%s) for writing!\n", argv[2]);
      usage();
    }
    /* Fall through */
  case 2:
    memory = atoi(argv[1]);
    if (!memory) {
      fprintf(stderr, "Machine requires memory to run, '%s is not a valid memory value!\n", argv[1]);
      usage();
    }
    break;
  default:
    usage();
    return 1;
  }

  struct dbnz_macro *macrolist;
  struct dbnz_statementlist stmtlist;

  char linebuf[LINEBUF_LEN];
  memset(linebuf, '\0', LINEBUF_LEN);

  struct dbnz_parse_state parse_state;
  memset(&parse_state, '\0', sizeof(struct dbnz_parse_state));
  parse_state.linebuf = linebuf;

  yyscan_t scanner;
  yylex_init(&scanner);
  yyset_extra(&parse_state, scanner);
  yyset_in(input, scanner);

#ifdef ENABLE_TRACE
  yyset_debug(1, scanner);
#endif

  int res = yyparse(scanner, &macrolist, &stmtlist);
  yylex_destroy(scanner);

  if (res) {
    return res; /* 1 - parse error, 2 - out of memory */
  }

  struct dbnz_compile_state *compile_state = dbnz_compile_program(&stmtlist, macrolist, memory);
  dbnz_write_all(output, compile_state, parse_state.source_files, parse_state.source_file_num);

  /* XXX: Assumes all memory is cleared on SIGSEG... er, I mean program exit. :) */
  return 0;
}
