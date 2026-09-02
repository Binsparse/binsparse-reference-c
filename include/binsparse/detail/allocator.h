/*
 * SPDX-FileCopyrightText: 2024 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <stdlib.h>

typedef struct bsp_allocator_t {
  void* (*malloc)(size_t);
  void (*free)(void*);
} bsp_allocator_t;

const static bsp_allocator_t bsp_default_allocator = {malloc, free};
