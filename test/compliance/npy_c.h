/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void* data;
  size_t size;
  size_t word_size;
  size_t rank;
  size_t* shape;
  size_t* strides;
} npy_c_array;

int npy_c_load(const char* path, npy_c_array* array);
void npy_c_destroy(npy_c_array* array);
int npy_c_save(const char* path, const void* data, const size_t* shape,
               size_t rank, int bsp_type);

#ifdef __cplusplus
}
#endif
