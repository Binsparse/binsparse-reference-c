/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <binsparse/error.h>

#define BINSPARSE_VERSION "0.1.0"

#ifdef __cplusplus
extern "C" {
#endif

bsp_error_t bsp_check_version_compatible(const char* version);

#ifdef __cplusplus
}
#endif
