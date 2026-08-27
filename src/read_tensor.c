/*
 * SPDX-FileCopyrightText: 2024 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <binsparse/hdf5_wrapper.h>
#include <binsparse/matrix.h>
#include <binsparse/matrix_market/matrix_market_read.h>
#include <binsparse/tensor.h>
#include <cJSON/cJSON.h>
#include <math.h>
#include <string.h>

/*
Returns "{key}-{index}"
*/
char* key_with_index(const char* key, size_t index) {
  int keylen = strlen(key);
  int strsize = keylen * sizeof(char) +
                (int) ((ceil(log10(index + 1)) + 1) * sizeof(char)) + 1;
  char* res = (char*) malloc(strsize);
  for (int i = 0; i < keylen; i++) {
    res[i] = key[i];
  }
  sprintf(res + keylen, "%ld", index);
  res[strsize] = '\0';
  return res;
}

bsp_tensor_t bsp_read_tensor_from_group(hid_t f) {
  bsp_tensor_t tensor = bsp_construct_default_tensor_t();

  char* json_string;
  bsp_error_t error = bsp_read_attribute(&json_string, f, (char*) "binsparse");
  if (error != BSP_SUCCESS) {
    return tensor;
  }

  cJSON* j = cJSON_Parse(json_string);

  assert(j != NULL);
  assert(cJSON_IsObject(j));

  cJSON* binsparse = cJSON_GetObjectItemCaseSensitive(j, "binsparse");
  assert(cJSON_IsObject(binsparse));

  cJSON* version_ = cJSON_GetObjectItemCaseSensitive(binsparse, "version");

  assert(version_ != NULL);

  assert(cJSON_IsString(version_));

  // nnz computation
  cJSON* nnz_ =
      cJSON_GetObjectItemCaseSensitive(binsparse, "number_of_stored_values");
  assert(nnz_ != NULL);
  size_t nnz = cJSON_GetNumberValue(nnz_);
  tensor.nnz = nnz;

  // check tensor shape.
  cJSON* shape_ = cJSON_GetObjectItemCaseSensitive(binsparse, "shape");
  assert(shape_ != NULL);
  tensor.rank = cJSON_GetArraySize(shape_);
  size_t* dims = (size_t*) malloc(tensor.rank * sizeof(size_t));
  for (int idx = 0; idx < tensor.rank; idx++) {
    dims[idx] = cJSON_GetNumberValue(cJSON_GetArrayItem(shape_, idx));
  }
  tensor.dims = dims;

  cJSON* data_types_ =
      cJSON_GetObjectItemCaseSensitive(binsparse, "data_types");
  assert(data_types_ != NULL);
  const char* values_type = cJSON_GetStringValue(
      cJSON_GetObjectItemCaseSensitive(data_types_, "values"));
  assert(values_type != NULL);
  if (strncmp(values_type, "iso[", 4) == 0) {
    tensor.is_iso = true;
    values_type += 4;
  }

  cJSON* binsparse_custom =
      cJSON_GetObjectItemCaseSensitive(binsparse, "custom");
  assert(binsparse_custom != NULL);

  cJSON* transpose_ =
      cJSON_GetObjectItemCaseSensitive(binsparse_custom, "transpose");
  if (transpose_ != NULL) {
    size_t* transpose = (size_t*) malloc(tensor.rank * sizeof(size_t));
    for (int idx = 0; idx < tensor.rank; idx++) {
      transpose[idx] =
          cJSON_GetNumberValue((cJSON_GetArrayItem(transpose_, idx)));
    }
    tensor.transpose = transpose;
  }

  cJSON* json_level =
      cJSON_GetObjectItemCaseSensitive(binsparse_custom, "level");
  assert(json_level != NULL);

  bsp_level_t* cur_level = malloc(sizeof(bsp_level_t));
  tensor.level = cur_level;

  // this is effectively a pointer on dims.
  size_t depth = 0;

  while (depth < tensor.rank + 1) {
    cJSON* type_object =
        cJSON_GetObjectItemCaseSensitive(json_level, "level_desc");
    char* type = type_object ? cJSON_GetStringValue(type_object) : NULL;
    assert(type != NULL);

    // base case: working with an element.
    if (strcmp(type, "element") == 0) {
      bsp_array_t values;
      bsp_error_t error = bsp_read_array(&values, f, (char*) "values");
      if (error != BSP_SUCCESS) {
        free(json_string);
        return tensor;
      }
      if (strncmp(values_type, "complex[", 8) == 0) {
        error = bsp_fp_array_to_complex(&values);
        if (error != BSP_SUCCESS) {
          bsp_destroy_array_t(&values);
          free(json_string);
          return tensor;
        }
      } else if (strncmp(values_type, "bint8", 5) == 0) {
        values.type = BSP_BINT8;
      }
      cur_level->kind = BSP_TENSOR_ELEMENT;
      bsp_element_t* data = malloc(sizeof(bsp_element_t));
      data->values = values;
      cur_level->data = data;
      depth++;
      break;
    }

    // compute what the rank of our current level is, and update our pointer
    // accordingly.
    cJSON* rank_obj = cJSON_GetObjectItemCaseSensitive(json_level, "rank");
    int rank = cJSON_GetNumberValue(rank_obj);

    if (strcmp(type, "dense") == 0) {
      cur_level->kind = BSP_TENSOR_DENSE;

      bsp_dense_t* data = malloc(sizeof(bsp_dense_t));
      data->rank = rank;
      data->child = malloc(sizeof(bsp_level_t));

      cur_level->data = data;
      cur_level = data->child;

    } else if (strcmp(type, "sparse") == 0) {
      cur_level->kind = BSP_TENSOR_SPARSE;

      bsp_sparse_t* data = malloc(sizeof(bsp_sparse_t));

      // initialize pointers_to, but only if the depth is not zero.
      if (depth != 0) {
        char* pointers_key = key_with_index("pointers_to_", depth);
        data->pointers_to = malloc(sizeof(bsp_array_t));
        error = bsp_read_array(data->pointers_to, f, pointers_key);
        if (error != BSP_SUCCESS) {
          free(pointers_key);
          free(json_string);
          return tensor;
        }
        free(pointers_key);
      } else {
        data->pointers_to = NULL;
      }

      // initialize indices
      data->indices = malloc(rank * sizeof(bsp_array_t));
      for (int idx = 0; idx < rank; idx++) {
        char* indices_key = key_with_index("indices_", depth + idx);
        error = bsp_read_array(&data->indices[idx], f, indices_key);
        if (error != BSP_SUCCESS) {
          free(indices_key);
          free(json_string);
          return tensor;
        }
        free(indices_key);
      }

      data->rank = rank;
      data->child = malloc(sizeof(bsp_level_t));
      cur_level->data = data;
      cur_level = data->child;

    } else {
      assert(false);
    }
    // update the depth here.
    json_level = cJSON_GetObjectItemCaseSensitive(json_level, "level");
    assert(json_level != NULL);
    depth += rank;
  }
  if (cJSON_HasObjectItem(binsparse, "structure")) {
    cJSON* structure_ =
        cJSON_GetObjectItemCaseSensitive(binsparse, "structure");
    char* structure = cJSON_GetStringValue(structure_);
    tensor.structure = bsp_get_structure(structure);
  }

  cJSON_Delete(j);
  free(json_string);

  return tensor;
}

static inline size_t bsp_final_dot(const char* str) {
  size_t dot_idx = 0;
  for (size_t i = 0; str[i] != '\0'; i++) {
    if (str[i] == '.') {
      dot_idx = i;
    }
  }
  return dot_idx;
}

bsp_tensor_t bsp_read_tensor(const char* file_name, const char* group) {
  if (group == NULL) {
    size_t idx = bsp_final_dot(file_name);
    if (strcmp(file_name + idx, ".hdf5") == 0 ||
        strcmp(file_name + idx, ".h5") == 0) {
      hid_t f = H5Fopen(file_name, H5F_ACC_RDONLY, H5P_DEFAULT);
      bsp_tensor_t tensor = bsp_read_tensor_from_group(f);
      H5Fclose(f);
      return tensor;
    } else {
      assert(false);
    }
  } else {
    hid_t f = H5Fopen(file_name, H5F_ACC_RDONLY, H5P_DEFAULT);
    hid_t g = H5Gopen1(f, group);
    bsp_tensor_t matrix = bsp_read_tensor_from_group(g);
    H5Gclose(g);
    H5Fclose(f);
    return matrix;
  }
}
