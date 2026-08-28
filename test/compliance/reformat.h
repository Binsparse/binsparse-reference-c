/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <binsparse/array.h>
#include <binsparse/error.h>
#include <binsparse/matrix.h>
#include <binsparse/read_matrix.h>
#include <binsparse/read_tensor.h>
#include <binsparse/tensor.h>
#include <binsparse/write_matrix.h>
#include <binsparse/write_tensor.h>
#include <cJSON/cJSON.h>

typedef struct {
  size_t* coord;
  size_t value;
} entry_t;

typedef struct {
  entry_t* entry;
  size_t size;
  size_t capacity;
  int rank;
} entries_t;

typedef struct {
  cJSON* data_types;
} reformat_context_t;

void die(const char* message);
cJSON* read_json(const char* path);
cJSON* input_header(const char* path);
bool header_values_are_iso(cJSON* header);
bool header_has_fill(cJSON* header);
void complete_output(const char* path, cJSON* requested,
                     const bsp_array_t* fill_value);

bsp_type_t type_from_name(const char* name);
void push_entry(entries_t* entries, const size_t* coord, size_t value);
void free_entries(entries_t* entries);
entries_t tensor_to_coo(const bsp_tensor_t* tensor);
entries_t matrix_to_coo(bsp_matrix_t* matrix);
void apply_transpose(entries_t* entries, const size_t* source,
                     const size_t* target);
bsp_array_t values_in_entry_order(const bsp_array_t source,
                                  const entries_t* entries, bool iso);

cJSON* predefined_level(const char* format, size_t* transpose, int rank);
bsp_level_t* build_level(const reformat_context_t* context, cJSON* description,
                         const entries_t* entries, const size_t* dims,
                         size_t depth, const size_t* parent_ptr, size_t parents,
                         bool root, bsp_array_t values);
bsp_error_t write_predefined(const char* path, bsp_tensor_t tensor,
                             const char* format, int compression);

bsp_error_t bsp_reformat_file(const char* input, const char* output,
                              cJSON* target_header, int compression);
