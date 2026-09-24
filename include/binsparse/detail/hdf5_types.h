/*
 * SPDX-FileCopyrightText: 2024 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <binsparse/types.h>
#include <hdf5.h>

static inline hid_t bsp_get_hdf5_standard_type(bsp_type_t type) {
  if (type == BSP_UINT8) {
    return H5T_STD_U8LE;
  } else if (type == BSP_UINT16) {
    return H5T_STD_U16LE;
  } else if (type == BSP_UINT32) {
    return H5T_STD_U32LE;
  } else if (type == BSP_UINT64) {
    return H5T_STD_U64LE;
  } else if (type == BSP_INT8) {
    return H5T_STD_I8LE;
  } else if (type == BSP_INT16) {
    return H5T_STD_I16LE;
  } else if (type == BSP_INT32) {
    return H5T_STD_I32LE;
  } else if (type == BSP_INT64) {
    return H5T_STD_I64LE;
  } else if (type == BSP_FLOAT32) {
    return H5T_IEEE_F32LE;
  } else if (type == BSP_FLOAT64) {
    return H5T_IEEE_F64LE;
  } else if (type == BSP_BINT8) {
    return H5T_STD_U8LE;
  } else {
    return H5I_INVALID_HID;
  }
}

static inline bsp_type_t bsp_get_bsp_type(hid_t type) {
  H5T_class_t cl = H5Tget_class(type);
  H5T_order_t order = H5Tget_order(type);
  H5T_sign_t sign = H5Tget_sign(type);
  size_t size = H5Tget_size(type);

  if (cl == H5T_INTEGER) {
    if (sign == H5T_SGN_NONE) {
      if (size == 1) {
        return BSP_UINT8;
      } else if (size == 2) {
        return BSP_UINT16;
      } else if (size == 4) {
        return BSP_UINT32;
      } else if (size == 8) {
        return BSP_UINT64;
      } else {
        return BSP_INVALID_TYPE;
      }
    } else /* if (sign == H5T_SGN_2) */ {
      if (size == 1) {
        return BSP_INT8;
      } else if (size == 2) {
        return BSP_INT16;
      } else if (size == 4) {
        return BSP_INT32;
      } else if (size == 8) {
        return BSP_INT64;
      } else {
        return BSP_INVALID_TYPE;
      }
    }
  } else if (cl == H5T_FLOAT) {
    if (size == 4) {
      return BSP_FLOAT32;
    } else if (size == 8) {
      return BSP_FLOAT64;
    } else {
      return BSP_INVALID_TYPE;
    }
  } else {
    return BSP_INVALID_TYPE;
  }
}

// NOTE: This code is a bit silly, but it seems to be the only
//       way to generically determine the HDF5 native types for
//       stdint's fixed width integer types.
static inline hid_t bsp_get_hdf5_native_type(bsp_type_t type) {
  if (type == BSP_INT8) {
    if (sizeof(int8_t) == sizeof(char)) {
      return H5T_NATIVE_CHAR;
    } else if (sizeof(int8_t) == sizeof(short)) {
      return H5T_NATIVE_SHORT;
    } else if (sizeof(int8_t) == sizeof(int)) {
      return H5T_NATIVE_INT;
    } else if (sizeof(int8_t) == sizeof(long)) {
      return H5T_NATIVE_LONG;
    } else if (sizeof(int8_t) == sizeof(long long)) {
      return H5T_NATIVE_LLONG;
    } else {
      assert(false);
    }
  } else if (type == BSP_INT16) {
    if (sizeof(int16_t) == sizeof(char)) {
      return H5T_NATIVE_CHAR;
    } else if (sizeof(int16_t) == sizeof(short)) {
      return H5T_NATIVE_SHORT;
    } else if (sizeof(int16_t) == sizeof(int)) {
      return H5T_NATIVE_INT;
    } else if (sizeof(int32_t) == sizeof(long)) {
      return H5T_NATIVE_LONG;
    } else if (sizeof(int64_t) == sizeof(long long)) {
      return H5T_NATIVE_LLONG;
    } else {
      assert(false);
    }
  } else if (type == BSP_INT32) {
    if (sizeof(int32_t) == sizeof(char)) {
      return H5T_NATIVE_CHAR;
    } else if (sizeof(int32_t) == sizeof(short)) {
      return H5T_NATIVE_SHORT;
    } else if (sizeof(int32_t) == sizeof(int)) {
      return H5T_NATIVE_INT;
    } else if (sizeof(int32_t) == sizeof(long)) {
      return H5T_NATIVE_LONG;
    } else if (sizeof(int32_t) == sizeof(long long)) {
      return H5T_NATIVE_LLONG;
    } else {
      assert(false);
    }
  } else if (type == BSP_INT64) {
    if (sizeof(int64_t) == sizeof(char)) {
      return H5T_NATIVE_CHAR;
    } else if (sizeof(int64_t) == sizeof(short)) {
      return H5T_NATIVE_SHORT;
    } else if (sizeof(int64_t) == sizeof(int)) {
      return H5T_NATIVE_INT;
    } else if (sizeof(int64_t) == sizeof(long)) {
      return H5T_NATIVE_LONG;
    } else if (sizeof(int64_t) == sizeof(long long)) {
      return H5T_NATIVE_LLONG;
    } else {
      assert(false);
    }
  } else if (type == BSP_UINT8 || type == BSP_BINT8) {
    if (sizeof(uint8_t) == sizeof(unsigned char)) {
      return H5T_NATIVE_UCHAR;
    } else if (sizeof(uint8_t) == sizeof(unsigned short)) {
      return H5T_NATIVE_USHORT;
    } else if (sizeof(uint8_t) == sizeof(unsigned int)) {
      return H5T_NATIVE_UINT;
    } else if (sizeof(uint8_t) == sizeof(unsigned long)) {
      return H5T_NATIVE_ULONG;
    } else if (sizeof(uint8_t) == sizeof(unsigned long long)) {
      return H5T_NATIVE_ULLONG;
    } else {
      assert(false);
    }
  } else if (type == BSP_UINT16) {
    if (sizeof(uint16_t) == sizeof(unsigned char)) {
      return H5T_NATIVE_UCHAR;
    } else if (sizeof(uint16_t) == sizeof(unsigned short)) {
      return H5T_NATIVE_USHORT;
    } else if (sizeof(uint16_t) == sizeof(unsigned int)) {
      return H5T_NATIVE_UINT;
    } else if (sizeof(uint16_t) == sizeof(unsigned long)) {
      return H5T_NATIVE_ULONG;
    } else if (sizeof(uint16_t) == sizeof(unsigned long long)) {
      return H5T_NATIVE_ULLONG;
    } else {
      assert(false);
    }
  } else if (type == BSP_UINT32) {
    if (sizeof(uint32_t) == sizeof(unsigned char)) {
      return H5T_NATIVE_UCHAR;
    } else if (sizeof(uint32_t) == sizeof(unsigned short)) {
      return H5T_NATIVE_USHORT;
    } else if (sizeof(uint32_t) == sizeof(unsigned int)) {
      return H5T_NATIVE_UINT;
    } else if (sizeof(uint32_t) == sizeof(unsigned long)) {
      return H5T_NATIVE_ULONG;
    } else if (sizeof(uint32_t) == sizeof(unsigned long long)) {
      return H5T_NATIVE_ULLONG;
    } else {
      assert(false);
    }
  } else if (type == BSP_UINT64) {
    if (sizeof(uint64_t) == sizeof(unsigned char)) {
      return H5T_NATIVE_UCHAR;
    } else if (sizeof(uint64_t) == sizeof(unsigned short)) {
      return H5T_NATIVE_USHORT;
    } else if (sizeof(uint64_t) == sizeof(unsigned int)) {
      return H5T_NATIVE_UINT;
    } else if (sizeof(uint64_t) == sizeof(unsigned long)) {
      return H5T_NATIVE_ULONG;
    } else if (sizeof(uint64_t) == sizeof(unsigned long long)) {
      return H5T_NATIVE_ULLONG;
    } else {
      assert(false);
    }
  } else if (type == BSP_FLOAT32) {
    return H5T_NATIVE_FLOAT;
  } else if (type == BSP_FLOAT64) {
    return H5T_NATIVE_DOUBLE;
  } else {
    return H5I_INVALID_HID;
  }
}
