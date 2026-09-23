// SPDX-FileCopyrightText: 2024 Binsparse Developers
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#ifdef __cplusplus

#include <string>
#include <type_traits>
#include <variant>

namespace binsparse {

namespace __detail {

using array_ptr_variant_t =
    std::variant<uint8_t*, uint16_t*, uint32_t*, uint64_t*, int8_t*, int16_t*,
                 int32_t*, int64_t*, float*, double*, float _Complex*,
                 double _Complex*>;

inline array_ptr_variant_t get_typed_ptr(bsp_array_t array) {
  if (array.type == BSP_UINT8) {
    uint8_t* data = (uint8_t*) array.data;
    return data;
  } else if (array.type == BSP_UINT16) {
    uint16_t* data = (uint16_t*) array.data;
    return data;
  } else if (array.type == BSP_UINT32) {
    uint32_t* data = (uint32_t*) array.data;
    return data;
  } else if (array.type == BSP_UINT64) {
    uint64_t* data = (uint64_t*) array.data;
    return data;
  } else if (array.type == BSP_INT8) {
    int8_t* data = (int8_t*) array.data;
    return data;
  } else if (array.type == BSP_INT16) {
    int16_t* data = (int16_t*) array.data;
    return data;
  } else if (array.type == BSP_INT32) {
    int32_t* data = (int32_t*) array.data;
    return data;
  } else if (array.type == BSP_INT64) {
    int64_t* data = (int64_t*) array.data;
    return data;
  } else if (array.type == BSP_FLOAT32) {
    float* data = (float*) array.data;
    return data;
  } else if (array.type == BSP_FLOAT64) {
    double* data = (double*) array.data;
    return data;
  } else if (array.type == BSP_BINT8) {
    int8_t* data = (int8_t*) array.data;
    return data;
  } else if (array.type == BSP_COMPLEX_FLOAT32) {
    float _Complex* data = (float _Complex*) array.data;
    return data;
  } else if (array.type == BSP_COMPLEX_FLOAT64) {
    double _Complex* data = (double _Complex*) array.data;
    return data;
  }
  return {};
}

template <typename T>
constexpr static bool is_c_complex_v =
    std::is_same_v<std::remove_cvref_t<T>, _Complex double> ||
    std::is_same_v<std::remove_cvref_t<T>, _Complex float>;

template <typename T, typename U>
constexpr static bool mixed_c_complex_v =
    (is_c_complex_v<T> && !is_c_complex_v<U>) ||
    (!is_c_complex_v<T> && is_c_complex_v<U>);

} // namespace __detail

} // namespace binsparse

// value = array[index]
template <typename T>
inline void bsp_array_read(bsp_array_t array, size_t index, T& value) {
  auto variant_ptr = binsparse::__detail::get_typed_ptr(array);

  std::visit(
      [&](auto* ptr) {
        using U = std::remove_pointer_t<decltype(ptr)>;

        if constexpr (std::is_convertible_v<U, T> &&
                      !binsparse::__detail::mixed_c_complex_v<T, U>) {
          value = ptr[index];
        }
      },
      variant_ptr);
}

// array[index] = value
template <typename U>
inline void bsp_array_write(bsp_array_t array, size_t index, U value) {
  auto variant_ptr = binsparse::__detail::get_typed_ptr(array);

  std::visit(
      [&](auto* ptr) {
        using T = std::remove_pointer_t<decltype(ptr)>;

        if constexpr (std::is_convertible_v<U, T> &&
                      !binsparse::__detail::mixed_c_complex_v<T, U>) {
          ptr[index] = value;
        }
      },
      variant_ptr);
}

// array_0[index_0] = array_1[index_1]
inline void bsp_array_awrite(bsp_array_t array_0, size_t index_0,
                             bsp_array_t array_1, size_t index_1) {
  auto variant_ptr_0 = binsparse::__detail::get_typed_ptr(array_0);
  auto variant_ptr_1 = binsparse::__detail::get_typed_ptr(array_1);

  std::visit(
      [&](auto* ptr_0, auto* ptr_1) {
        using T = std::remove_pointer_t<decltype(ptr_0)>;
        using U = std::remove_pointer_t<decltype(ptr_1)>;

        if constexpr (std::is_convertible_v<U, T> &&
                      !binsparse::__detail::mixed_c_complex_v<T, U>) {
          ptr_0[index_0] = ptr_1[index_1];
        }
      },
      variant_ptr_0, variant_ptr_1);
}

#endif
