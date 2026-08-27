#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void *data;
  size_t size;
  size_t word_size;
  size_t rank;
  size_t *shape;
  size_t *strides;
} cnpy_c_array;

int cnpy_c_load(const char *path, cnpy_c_array *array);
void cnpy_c_destroy(cnpy_c_array *array);
int cnpy_c_save(const char *path, const void *data, const size_t *shape,
                size_t rank, int bsp_type);

#ifdef __cplusplus
}
#endif
