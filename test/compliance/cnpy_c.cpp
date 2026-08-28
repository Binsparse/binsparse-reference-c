// SPDX-FileCopyrightText: 2026 Binsparse Developers
//
// SPDX-License-Identifier: BSD-3-Clause

#include "cnpy_c.h"

#include <cnpy.h>

#include <complex>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

extern "C" int cnpy_c_load(const char* path, cnpy_c_array* out) {
  try {
    cnpy::NpyArray array = cnpy::npy_load(path);
    out->size = array.num_vals;
    out->word_size = array.word_size;
    out->rank = array.shape.size();
    out->shape = static_cast<size_t*>(malloc(out->rank * sizeof(size_t)));
    out->strides = static_cast<size_t*>(malloc(out->rank * sizeof(size_t)));
    out->data = malloc(array.num_bytes());
    if ((out->rank && (!out->shape || !out->strides)) ||
        (array.num_bytes() && !out->data))
      return 1;
    if (out->rank)
      memcpy(out->shape, array.shape.data(), out->rank * sizeof(size_t));
    size_t stride = 1;
    if (array.fortran_order) {
      for (size_t d = 0; d < out->rank; ++d) {
        out->strides[d] = stride;
        stride *= out->shape[d];
      }
    } else {
      for (size_t d = out->rank; d-- > 0;) {
        out->strides[d] = stride;
        stride *= out->shape[d];
      }
    }
    if (array.num_bytes())
      memcpy(out->data, array.data<char>(), array.num_bytes());
    return 0;
  } catch (const std::exception&) {
    return 1;
  }
}

extern "C" void cnpy_c_destroy(cnpy_c_array* array) {
  free(array->data);
  free(array->shape);
  free(array->strides);
  memset(array, 0, sizeof(*array));
}

template <typename T>
static void save(const char* path, const void* data, const size_t* shape,
                 size_t rank) {
  std::vector<size_t> dimensions;
  if (rank != 0)
    dimensions.assign(shape, shape + rank);
  cnpy::npy_save(path, static_cast<const T*>(data), dimensions);
}

extern "C" int cnpy_c_save(const char* path, const void* data,
                           const size_t* shape, size_t rank, int type) {
  try {
    switch (type) {
    case 0:
      save<uint8_t>(path, data, shape, rank);
      break;
    case 1:
      save<uint16_t>(path, data, shape, rank);
      break;
    case 2:
      save<uint32_t>(path, data, shape, rank);
      break;
    case 3:
      save<uint64_t>(path, data, shape, rank);
      break;
    case 4:
      save<int8_t>(path, data, shape, rank);
      break;
    case 5:
      save<int16_t>(path, data, shape, rank);
      break;
    case 6:
      save<int32_t>(path, data, shape, rank);
      break;
    case 7:
      save<int64_t>(path, data, shape, rank);
      break;
    case 8:
      save<float>(path, data, shape, rank);
      break;
    case 9:
      save<double>(path, data, shape, rank);
      break;
    case 10:
      save<bool>(path, data, shape, rank);
      break;
    case 11:
      save<std::complex<float>>(path, data, shape, rank);
      break;
    case 12:
      save<std::complex<double>>(path, data, shape, rank);
      break;
    default:
      return 1;
    }
    return 0;
  } catch (const std::exception&) {
    return 1;
  }
}
