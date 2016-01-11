
#ifndef RDBNZ_WRITE_H
#define RDBNZ_WRITE_H

#include <stdio.h>

void dbnz_write_all(FILE *output, struct dbnz_compile_state *s, const struct dbnz_source *source_files, size_t source_file_num);

#endif /* RDBNZ_WRITE_H */
