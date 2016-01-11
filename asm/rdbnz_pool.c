
#include "rdbnz.h"
#include "rdbnz_pool.h"

/* Find or add a constant to the constant pool */
size_t dbnz_pool_constant(struct dbnz_compile_state *s, size_t value) {
  size_t i;
  for (i = 0; i != s->constant_num; ++i) {
    if (s->constants[i] == value) {
      return i;
    }
  }
  if (s->constant_num == s->constant_max) {
    s->constants = realloc(s->constants, sizeof(size_t) * (s->constant_max += 10));
  }
  s->constants[s->constant_num] = value;
  return s->constant_num++;
}
