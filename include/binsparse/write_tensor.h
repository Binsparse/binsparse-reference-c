/*
 * SPDX-FileCopyrightText: 2024 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <binsparse/tensor.h>

#ifdef BSP_USE_HDF5
#include <hdf5.h>

bsp_error_t bsp_write_tensor_to_group(hid_t f, bsp_tensor_t tensor,
                                      const char* user_json,
                                      int compression_level);
#endif

bsp_error_t bsp_write_tensor(const char* fname, bsp_tensor_t tensor,
                             const char* group, const char* user_json,
                             int compression_level);

#ifdef __cplusplus
}
#endif
