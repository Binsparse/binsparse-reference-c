/*
 * SPDX-FileCopyrightText: 2024 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <binsparse/detail/allocator.h>
#include <binsparse/detail/shm_tools.h>
#include <binsparse/error.h>
#include <binsparse/types.h>
#include <stdlib.h>
#include <string.h>

typedef struct bsp_array_t {
  void* data;
  size_t size;
  bsp_type_t type;
  bsp_allocator_t allocator;
} bsp_array_t;

static inline bsp_error_t
bsp_construct_default_array_t_allocator(bsp_array_t* array,
                                        bsp_allocator_t allocator) {
  array->data = NULL;
  array->size = 0;
  array->type = BSP_INVALID_TYPE;
  array->allocator = allocator;
  return BSP_SUCCESS;
}

static inline bsp_error_t bsp_construct_default_array_t(bsp_array_t* array) {
  return bsp_construct_default_array_t_allocator(array, bsp_default_allocator);
}

static inline bsp_error_t
bsp_construct_array_t_allocator(bsp_array_t* array, size_t size,
                                bsp_type_t type, bsp_allocator_t allocator) {
  size_t byte_size = size * bsp_type_size(type);

  array->allocator = allocator;
  array->data = byte_size == 0 ? NULL : array->allocator.malloc(byte_size);

  if (byte_size != 0 && array->data == NULL) {
    return BSP_ERROR_MEMORY;
  }

  array->size = size;
  array->type = type;

  return BSP_SUCCESS;
}

static inline bsp_error_t bsp_construct_array_t(bsp_array_t* array, size_t size,
                                                bsp_type_t type) {
  return bsp_construct_array_t_allocator(array, size, type,
                                         bsp_default_allocator);
}

static inline bsp_error_t bsp_copy_construct_array_t(bsp_array_t* array,
                                                     bsp_array_t other) {
  bsp_error_t error = bsp_construct_array_t(array, other.size, other.type);
  if (error != BSP_SUCCESS) {
    return error;
  }

  if (other.size != 0)
    memcpy(array->data, other.data, other.size * bsp_type_size(other.type));

  return BSP_SUCCESS;
}

static inline bsp_error_t bsp_complex_array_to_fp(bsp_array_t* array) {
  if (array->type != BSP_COMPLEX_FLOAT32 &&
      array->type != BSP_COMPLEX_FLOAT64) {
    return BSP_ERROR_TYPE;
  }

  array->size = array->size * 2;

  if (array->type == BSP_COMPLEX_FLOAT32) {
    array->type = BSP_FLOAT32;
  } else {
    array->type = BSP_FLOAT64;
  }

  return BSP_SUCCESS;
}

static inline bsp_error_t bsp_fp_array_to_complex(bsp_array_t* array) {
  if (array->type != BSP_FLOAT32 && array->type != BSP_FLOAT64) {
    return BSP_ERROR_TYPE;
  }

  if (array->type == BSP_FLOAT32) {
    array->type = BSP_COMPLEX_FLOAT32;
  } else {
    array->type = BSP_COMPLEX_FLOAT64;
  }

  array->size = array->size / 2;

  return BSP_SUCCESS;
}

static inline void bsp_destroy_array_t(bsp_array_t* array) {
  if (array->data != NULL) {
    array->allocator.free(array->data);
  }
}

static inline bool bsp_array_equal(bsp_array_t x, bsp_array_t y) {
  if (x.size != y.size) {
    return false;
  }

  if (x.size == 0) {
    return true;
  }

  if (x.type != y.type) {
    return false;
  }

  uint8_t* x_data = (uint8_t*) x.data;
  uint8_t* y_data = (uint8_t*) y.data;
  for (size_t i = 0; i < x.size * bsp_type_size(x.type); i++) {
    if (x_data[i] != y_data[i]) {
      printf("Index %zu incorrect %d != %d\n", i, (int) x_data[i],
             (int) y_data[i]);
      fflush(stdout);
      return false;
    }
  }
  return true;
}

#ifndef __cplusplus

#define bsp_array_block(array, ptr, code_block)                                \
  {                                                                            \
    if (array.type == BSP_UINT8) {                                             \
      uint8_t* ptr = (uint8_t*) array.data;                                    \
      code_block;                                                              \
    } else if (array.type == BSP_UINT16) {                                     \
      uint16_t* ptr = (uint16_t*) array.data;                                  \
      code_block;                                                              \
    } else if (array.type == BSP_UINT32) {                                     \
      uint32_t* ptr = (uint32_t*) array.data;                                  \
      code_block;                                                              \
    } else if (array.type == BSP_UINT64) {                                     \
      uint64_t* ptr = (uint64_t*) array.data;                                  \
      code_block;                                                              \
    } else if (array.type == BSP_INT8) {                                       \
      int8_t* ptr = (int8_t*) array.data;                                      \
      code_block;                                                              \
    } else if (array.type == BSP_INT16) {                                      \
      int16_t* ptr = (int16_t*) array.data;                                    \
      code_block;                                                              \
    } else if (array.type == BSP_INT32) {                                      \
      int32_t* ptr = (int32_t*) array.data;                                    \
      code_block;                                                              \
    } else if (array.type == BSP_INT64) {                                      \
      int64_t* ptr = (int64_t*) array.data;                                    \
      code_block;                                                              \
    } else if (array.type == BSP_FLOAT32) {                                    \
      float* ptr = (float*) array.data;                                        \
      code_block;                                                              \
    } else if (array.type == BSP_FLOAT64) {                                    \
      double* ptr = (double*) array.data;                                      \
      code_block;                                                              \
    } else if (array.type == BSP_BINT8) {                                      \
      int8_t* ptr = (int8_t*) array.data;                                      \
      code_block;                                                              \
    } else if (array.type == BSP_COMPLEX_FLOAT32) {                            \
      float _Complex* ptr = (float _Complex*) array.data;                      \
      code_block;                                                              \
    } else if (array.type == BSP_COMPLEX_FLOAT64) {                            \
      double _Complex* ptr = (double _Complex*) array.data;                    \
      code_block;                                                              \
    }                                                                          \
  }

// array[index] = value
#define bsp_array_write(array, index, value)                                   \
  {                                                                            \
    if (array.type == BSP_UINT8) {                                             \
      uint8_t* data = (uint8_t*) array.data;                                   \
      data[index] = value;                                                     \
    } else if (array.type == BSP_UINT16) {                                     \
      uint16_t* data = (uint16_t*) array.data;                                 \
      data[index] = value;                                                     \
    } else if (array.type == BSP_UINT32) {                                     \
      uint32_t* data = (uint32_t*) array.data;                                 \
      data[index] = value;                                                     \
    } else if (array.type == BSP_UINT64) {                                     \
      uint64_t* data = (uint64_t*) array.data;                                 \
      data[index] = value;                                                     \
    } else if (array.type == BSP_INT8) {                                       \
      int8_t* data = (int8_t*) array.data;                                     \
      data[index] = value;                                                     \
    } else if (array.type == BSP_INT16) {                                      \
      int16_t* data = (int16_t*) array.data;                                   \
      data[index] = value;                                                     \
    } else if (array.type == BSP_INT32) {                                      \
      int32_t* data = (int32_t*) array.data;                                   \
      data[index] = value;                                                     \
    } else if (array.type == BSP_INT64) {                                      \
      int64_t* data = (int64_t*) array.data;                                   \
      data[index] = value;                                                     \
    } else if (array.type == BSP_FLOAT32) {                                    \
      float* data = (float*) array.data;                                       \
      data[index] = value;                                                     \
    } else if (array.type == BSP_FLOAT64) {                                    \
      double* data = (double*) array.data;                                     \
      data[index] = value;                                                     \
    } else if (array.type == BSP_BINT8) {                                      \
      int8_t* data = (int8_t*) array.data;                                     \
      data[index] = ((size_t) value) % 2;                                      \
    } else if (array.type == BSP_COMPLEX_FLOAT32) {                            \
      float _Complex* data = (float _Complex*) array.data;                     \
      data[index] = value;                                                     \
    } else if (array.type == BSP_COMPLEX_FLOAT64) {                            \
      double _Complex* data = (double _Complex*) array.data;                   \
      data[index] = value;                                                     \
    }                                                                          \
  }

// array_0[index_0] = array1[index_1]
#define bsp_array_awrite(array_0, index_0, array_1, index_1)                   \
  {                                                                            \
    if (array_0.type == BSP_UINT8) {                                           \
      uint8_t* data0 = (uint8_t*) array_0.data;                                \
      uint8_t* data1 = (uint8_t*) array_1.data;                                \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_UINT16) {                                   \
      uint16_t* data0 = (uint16_t*) array_0.data;                              \
      uint16_t* data1 = (uint16_t*) array_1.data;                              \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_UINT32) {                                   \
      uint32_t* data0 = (uint32_t*) array_0.data;                              \
      uint32_t* data1 = (uint32_t*) array_1.data;                              \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_UINT64) {                                   \
      uint64_t* data0 = (uint64_t*) array_0.data;                              \
      uint64_t* data1 = (uint64_t*) array_1.data;                              \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_INT8) {                                     \
      int8_t* data0 = (int8_t*) array_0.data;                                  \
      int8_t* data1 = (int8_t*) array_1.data;                                  \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_INT16) {                                    \
      int16_t* data0 = (int16_t*) array_0.data;                                \
      int16_t* data1 = (int16_t*) array_1.data;                                \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_INT32) {                                    \
      int32_t* data0 = (int32_t*) array_0.data;                                \
      int32_t* data1 = (int32_t*) array_1.data;                                \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_INT64) {                                    \
      int64_t* data0 = (int64_t*) array_0.data;                                \
      int64_t* data1 = (int64_t*) array_1.data;                                \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_FLOAT32) {                                  \
      float* data0 = (float*) array_0.data;                                    \
      float* data1 = (float*) array_1.data;                                    \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_FLOAT64) {                                  \
      double* data0 = (double*) array_0.data;                                  \
      double* data1 = (double*) array_1.data;                                  \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_BINT8) {                                    \
      int8_t* data0 = (int8_t*) array_0.data;                                  \
      int8_t* data1 = (int8_t*) array_1.data;                                  \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_COMPLEX_FLOAT32) {                          \
      float _Complex* data0 = (float _Complex*) array_0.data;                  \
      float _Complex* data1 = (float _Complex*) array_1.data;                  \
      data0[index_0] = data1[index_1];                                         \
    } else if (array_0.type == BSP_COMPLEX_FLOAT64) {                          \
      double _Complex* data0 = (double _Complex*) array_0.data;                \
      double _Complex* data1 = (double _Complex*) array_1.data;                \
      data0[index_0] = data1[index_1];                                         \
    }                                                                          \
  }

// value = array[index]
#define bsp_array_read(array, index, value)                                    \
  {                                                                            \
    if (array.type == BSP_UINT8) {                                             \
      uint8_t* data = (uint8_t*) array.data;                                   \
      value = data[index];                                                     \
    } else if (array.type == BSP_UINT16) {                                     \
      uint16_t* data = (uint16_t*) array.data;                                 \
      value = data[index];                                                     \
    } else if (array.type == BSP_UINT32) {                                     \
      uint32_t* data = (uint32_t*) array.data;                                 \
      value = data[index];                                                     \
    } else if (array.type == BSP_UINT64) {                                     \
      uint64_t* data = (uint64_t*) array.data;                                 \
      value = data[index];                                                     \
    } else if (array.type == BSP_INT8) {                                       \
      int8_t* data = (int8_t*) array.data;                                     \
      value = data[index];                                                     \
    } else if (array.type == BSP_INT16) {                                      \
      int16_t* data = (int16_t*) array.data;                                   \
      value = data[index];                                                     \
    } else if (array.type == BSP_INT32) {                                      \
      int32_t* data = (int32_t*) array.data;                                   \
      value = data[index];                                                     \
    } else if (array.type == BSP_INT64) {                                      \
      int64_t* data = (int64_t*) array.data;                                   \
      value = data[index];                                                     \
    } else if (array.type == BSP_FLOAT32) {                                    \
      float* data = (float*) array.data;                                       \
      value = data[index];                                                     \
    } else if (array.type == BSP_FLOAT64) {                                    \
      double* data = (double*) array.data;                                     \
      value = data[index];                                                     \
    } else if (array.type == BSP_BINT8) {                                      \
      int8_t* data = (int8_t*) array.data;                                     \
      value = data[index];                                                     \
    } else if (array.type == BSP_COMPLEX_FLOAT32) {                            \
      float _Complex* data = (float _Complex*) array.data;                     \
      value = data[index];                                                     \
    } else if (array.type == BSP_COMPLEX_FLOAT64) {                            \
      double _Complex* data = (double _Complex*) array.data;                   \
      value = data[index];                                                     \
    }                                                                          \
  }

// swap(array[i], array[j])
#define bsp_array_swap(array, i, j)                                            \
  {                                                                            \
    if (array.type == BSP_UINT8) {                                             \
      uint8_t* data = (uint8_t*) array.data;                                   \
      uint8_t v = data[i];                                                     \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_UINT16) {                                     \
      uint16_t* data = (uint16_t*) array.data;                                 \
      uint16_t v = data[i];                                                    \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_UINT32) {                                     \
      uint32_t* data = (uint32_t*) array.data;                                 \
      uint32_t v = data[i];                                                    \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_UINT64) {                                     \
      uint64_t* data = (uint64_t*) array.data;                                 \
      uint64_t v = data[i];                                                    \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_INT8) {                                       \
      int8_t* data = (int8_t*) array.data;                                     \
      int8_t v = data[i];                                                      \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_INT16) {                                      \
      int16_t* data = (int16_t*) array.data;                                   \
      int16_t v = data[i];                                                     \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_INT32) {                                      \
      int32_t* data = (int32_t*) array.data;                                   \
      int32_t v = data[i];                                                     \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_INT64) {                                      \
      int64_t* data = (int64_t*) array.data;                                   \
      int64_t v = data[i];                                                     \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_FLOAT32) {                                    \
      float* data = (float*) array.data;                                       \
      float v = data[i];                                                       \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_FLOAT64) {                                    \
      double* data = (double*) array.data;                                     \
      double v = data[i];                                                      \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_BINT8) {                                      \
      int8_t* data = (int8_t*) array.data;                                     \
      int8_t v = data[i];                                                      \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_COMPLEX_FLOAT32) {                            \
      float _Complex* data = (float _Complex*) array.data;                     \
      float _Complex v = data[i];                                              \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    } else if (array.type == BSP_COMPLEX_FLOAT64) {                            \
      double _Complex* data = (double _Complex*) array.data;                   \
      double _Complex v = data[i];                                             \
      data[i] = data[j];                                                       \
      data[j] = v;                                                             \
    }                                                                          \
  }

#endif
