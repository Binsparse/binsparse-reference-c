/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "npy_c.h"
#include "reformat.h"

#include <binsparse/binsparse_cJSON.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t product(const size_t* shape, size_t rank) {
  size_t size = 1;
  for (size_t d = 0; d < rank; ++d)
    size *= shape[d];
  return size;
}

static size_t offset(const npy_c_array* array, const size_t* coord) {
  size_t result = 0;
  for (size_t d = 0; d < array->rank; ++d)
    result += coord[d] * array->strides[d];
  return result;
}

int main(int argc, char** argv) {
  npy_c_array dense = {0}, pattern = {0}, fill = {0};
  cJSON *header, *shape_json, *custom, *description;
  const char *format, *value_name;
  bsp_type_t value_type;
  bsp_array_t values;
  bsp_tensor_t result = bsp_construct_default_tensor_t();
  entries_t entries = {0};
  reformat_context_t context;
  size_t *logical_coord, *source_transpose, *target_transpose, *stored_dims,
      root_ptr[2];
  size_t stored = 0;
  bool iso;

  if (argc != 6) {
    fprintf(stderr, "usage: npy_to_binsparse <tensor_in> <pattern_in> "
                    "<fill_value_in> <header_in> <tensor_out>\n");
    return 2;
  }
  if (npy_c_load(argv[1], &dense) || npy_c_load(argv[2], &pattern) ||
      npy_c_load(argv[3], &fill))
    die("cannot read NPY input");
  if (dense.rank != pattern.rank || dense.size != pattern.size ||
      pattern.word_size != 1 || fill.size != 1)
    die("incompatible tensor, pattern, or fill-value NPY input");

  header = read_json(argv[4]);
  shape_json = cJSON_GetObjectItemCaseSensitive(header, "shape");
  result.rank = cJSON_GetArraySize(shape_json);
  if ((size_t) result.rank != dense.rank)
    die("header shape rank mismatch");
  result.dims =
      result.rank ? malloc((size_t) result.rank * sizeof(size_t)) : NULL;
  result.transpose =
      result.rank ? malloc((size_t) result.rank * sizeof(size_t)) : NULL;
  logical_coord =
      result.rank ? calloc((size_t) result.rank, sizeof(size_t)) : NULL;
  target_transpose =
      result.rank ? malloc((size_t) result.rank * sizeof(size_t)) : NULL;
  source_transpose =
      result.rank ? malloc((size_t) result.rank * sizeof(size_t)) : NULL;
  stored_dims =
      result.rank ? malloc((size_t) result.rank * sizeof(size_t)) : NULL;
  entries.rank = result.rank;
  for (int d = 0; d < result.rank; ++d) {
    result.dims[d] =
        (size_t) cJSON_GetNumberValue(cJSON_GetArrayItem(shape_json, d));
    if (result.dims[d] != dense.shape[d])
      die("header shape mismatch");
    source_transpose[d] = result.transpose[d] = target_transpose[d] =
        (size_t) d;
  }
  if (product(result.dims, (size_t) result.rank) != dense.size)
    die("NPY element count does not match shape");

  context.data_types = cJSON_GetObjectItemCaseSensitive(header, "data_types");
  value_name = cJSON_GetStringValue(
      cJSON_GetObjectItemCaseSensitive(context.data_types, "values"));
  value_type = type_from_name(value_name);
  if (value_type == BSP_INVALID_TYPE ||
      bsp_type_size(value_type) != dense.word_size)
    die("NPY dtype does not match header values type");
  if (fill.word_size != bsp_type_size(value_type))
    die("fill-value dtype does not match header values type");
  if (!cJSON_HasObjectItem(header, "fill"))
    cJSON_AddBoolToObject(header, "fill", true);
  if (!cJSON_HasObjectItem(context.data_types, "fill_value"))
    cJSON_AddStringToObject(context.data_types, "fill_value",
                            bsp_get_type_string(value_type));
  iso = value_name != NULL && !strncmp(value_name, "iso[", 4);
  for (size_t i = 0; i < pattern.size; ++i)
    if (((uint8_t*) pattern.data)[i])
      ++stored;
  if (bsp_construct_array_t(&values, iso && stored ? 1 : stored, value_type) !=
      BSP_SUCCESS)
    die("out of memory");

  size_t value_index = 0;
  for (size_t linear = 0; linear < dense.size; ++linear) {
    size_t dense_offset = offset(&dense, logical_coord);
    size_t pattern_offset = offset(&pattern, logical_coord);
    if (((uint8_t*) pattern.data)[pattern_offset]) {
      push_entry(&entries, logical_coord, iso ? 0 : value_index);
      if (!iso || value_index == 0)
        memcpy((char*) values.data + (iso ? 0 : value_index) * dense.word_size,
               (char*) dense.data + dense_offset * dense.word_size,
               dense.word_size);
      ++value_index;
    }
    for (int d = result.rank - 1; d >= 0; --d) {
      if (++logical_coord[d] < result.dims[d])
        break;
      logical_coord[d] = 0;
    }
  }
  result.nnz = stored;
  result.is_iso = iso;

  format =
      cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(header, "format"));
  if (format == NULL)
    die("header has no format");
  if (!strcmp(format, "custom")) {
    custom = cJSON_GetObjectItemCaseSensitive(header, "custom");
    description =
        cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(custom, "level"), 1);
    if (description == NULL)
      die("custom target requires custom.level");
    cJSON* transpose = cJSON_GetObjectItemCaseSensitive(custom, "transpose");
    if (transpose != NULL)
      for (int d = 0; d < result.rank; ++d)
        result.transpose[d] = target_transpose[d] =
            (size_t) cJSON_GetNumberValue(cJSON_GetArrayItem(transpose, d));
  } else {
    description = predefined_level(format, target_transpose, result.rank);
    if (description == NULL)
      die("unknown or rank-incompatible target format");
    for (int d = 0; d < result.rank; ++d)
      result.transpose[d] = target_transpose[d];
  }
  apply_transpose(&entries, source_transpose, target_transpose);
  bsp_array_t ordered_values = values_in_entry_order(values, &entries, iso);
  bsp_destroy_array_t(&values);
  values = ordered_values;
  for (int d = 0; d < result.rank; ++d)
    stored_dims[d] = result.dims[target_transpose[d]];
  root_ptr[0] = 0;
  root_ptr[1] = stored;
  result.level = build_level(&context, description, &entries, stored_dims, 0,
                             root_ptr, 1, true, values);
  if (!strcmp(format, "custom")) {
    cJSON* user = cJSON_CreateObject();
    if (bsp_write_tensor_cjson(argv[5], result, NULL, user, 0) != BSP_SUCCESS)
      die("cannot write Binsparse tensor");
    cJSON_Delete(user);
  } else if (write_predefined(argv[5], result, format, 0) != BSP_SUCCESS) {
    die("cannot write Binsparse tensor");
  }
  bsp_array_t fill_array;
  fill_array.data = fill.data;
  fill_array.size = 1;
  fill_array.type = value_type;
  fill_array.allocator = bsp_default_allocator;
  complete_output(argv[5], header, &fill_array);

  bsp_destroy_tensor_t(result);
  free_entries(&entries);
  free(logical_coord);
  free(source_transpose);
  free(target_transpose);
  free(stored_dims);
  cJSON_Delete(description);
  cJSON_Delete(header);
  npy_c_destroy(&dense);
  npy_c_destroy(&pattern);
  npy_c_destroy(&fill);
  return 0;
}
