/*
 * SPDX-FileCopyrightText: 2024 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <binsparse/binsparse.h>
#include <binsparse/binsparse_cJSON.h>
#include <binsparse/hdf5_wrapper.h>
#include <binsparse/read_tensor.h>
#include <binsparse/write_tensor.h>

#include <assert.h>
#include <unistd.h>

#include <cJSON/cJSON.h>
#include <hdf5.h>

static cJSON* init_tensor_json(bsp_tensor_t tensor, cJSON* user_json) {
  cJSON* j = cJSON_CreateObject();
  assert(j != NULL);

  cJSON* binsparse = cJSON_CreateObject();
  assert(binsparse != NULL);

  cJSON* binsparse_custom = cJSON_CreateObject();
  assert(binsparse_custom != NULL);

  cJSON_AddItemToObject(binsparse, "custom", binsparse_custom);
  cJSON_AddItemToObject(j, "binsparse", binsparse);

  if (user_json != NULL) {
    cJSON* user_json_item;
    cJSON_ArrayForEach(user_json_item, user_json) {
      cJSON* item_copy = cJSON_Duplicate(user_json_item, 1);
      cJSON_AddItemToObject(j, user_json_item->string, item_copy);
    }
  }

  cJSON_AddStringToObject(binsparse, "version", BINSPARSE_VERSION);

  cJSON* shape = cJSON_AddArrayToObject(binsparse, "shape");
  for (int i = 0; i < tensor.rank; i++) {
    cJSON_AddItemToArray(shape, cJSON_CreateNumber(tensor.dims[i]));
  }

  cJSON* transpose = cJSON_AddArrayToObject(binsparse_custom, "transpose");
  for (int i = 0; i < tensor.rank; i++) {
    cJSON_AddItemToArray(transpose, cJSON_CreateNumber(tensor.transpose[i]));
  }

  cJSON_AddNumberToObject(binsparse, "number_of_stored_values", tensor.nnz);

  if (tensor.structure != BSP_GENERAL) {
    cJSON_AddStringToObject(binsparse, "structure",
                            bsp_get_structure_string(tensor.structure));
  }

  return j;
}

bsp_error_t bsp_write_tensor_to_group_cjson(hid_t f, bsp_tensor_t tensor,
                                            cJSON* user_json,
                                            int compression_level) {
  // bsp_matrix_t matrix;
  cJSON* j = init_tensor_json(tensor, user_json);
  // tensor:
  cJSON* binsparse = cJSON_GetObjectItemCaseSensitive(j, "binsparse");
  assert(binsparse != NULL);
  cJSON* binsparse_custom =
      cJSON_GetObjectItemCaseSensitive(binsparse, "custom");
  assert(binsparse_custom != NULL);

  cJSON* data_types = cJSON_AddObjectToObject(binsparse, "data_types");
  bsp_array_t values = bsp_get_tensor_values(tensor);

  if (!tensor.is_iso) {
    cJSON_AddStringToObject(data_types, "values",
                            bsp_get_type_string(values.type));
  } else {
    char* base_type_string = bsp_get_type_string(values.type);
    size_t len = strlen(base_type_string) + 6;
    char* type_string = (char*) malloc(sizeof(char) * len);

    strncpy(type_string, "iso[", len);
    strncpy(type_string + 4, base_type_string, len - 4);
    strncpy(type_string + len - 2, "]", 2);

    cJSON_AddStringToObject(data_types, "values", type_string);

    free(type_string);
  }

  // attempt to write an array.
  bsp_error_t error =
      bsp_write_array(f, (char*) "values", values, compression_level);
  if (error != BSP_SUCCESS) {
    cJSON_Delete(j);
    return error;
  }

  int rank = 0;
  bsp_level_t* level = tensor.level;
  cJSON* json_level = cJSON_AddObjectToObject(binsparse_custom, "level");
  while (true) {
    int reached_end = 0;
    switch (level->kind) {
    case BSP_TENSOR_SPARSE: {
      bsp_sparse_t* sparse = level->data;
      size_t layer_rank = sparse->rank;
      cJSON_AddStringToObject(json_level, "level_desc", "sparse");
      cJSON_AddNumberToObject(json_level, "rank", layer_rank);

      if (sparse->pointers_to != NULL) {
        cJSON_AddStringToObject(data_types,
                                key_with_index("pointers_to_", rank),
                                bsp_get_type_string(sparse->pointers_to->type));
        error = bsp_write_array(f, key_with_index("pointers_to_", rank),
                                *sparse->pointers_to, compression_level);
        if (error != BSP_SUCCESS) {
          cJSON_Delete(j);
          return error;
        }
      }

      for (int i = 0; i < layer_rank; i++) {
        cJSON_AddStringToObject(data_types,
                                key_with_index("indices_", rank + i),
                                bsp_get_type_string(sparse->indices[i].type));
        error = bsp_write_array(f, key_with_index("indices_", rank + i),
                                sparse->indices[i], compression_level);
        if (error != BSP_SUCCESS) {
          cJSON_Delete(j);
          return error;
        }
      }

      rank += layer_rank;
      level = sparse->child;
      break;
    }
    case BSP_TENSOR_DENSE: {
      cJSON_AddStringToObject(json_level, "level_desc", "dense");
      cJSON_AddNumberToObject(json_level, "rank",
                              ((bsp_dense_t*) level->data)->rank);
      rank += ((bsp_dense_t*) level->data)->rank;
      level = ((bsp_dense_t*) level->data)->child;
      break;
    }
    case BSP_TENSOR_ELEMENT: {
      cJSON_AddStringToObject(json_level, "level_desc", "element");
      reached_end = 1;
      break;
    }
    default:;
    }

    if (reached_end)
      break;
    json_level = cJSON_AddObjectToObject(json_level, "level");
  }

  char* json_string = cJSON_Print(j);
  error = bsp_write_attribute(f, (char*) "binsparse", json_string);
  if (error != BSP_SUCCESS) {
    free(json_string);
    cJSON_Delete(j);
    return error;
  }
  free(json_string);

  return BSP_SUCCESS;
}

bsp_error_t bsp_write_tensor_to_group(hid_t f, bsp_tensor_t tensor,
                                      const char* user_json,
                                      int compression_level) {
  cJSON* user_json_cjson = NULL;
  if (user_json != NULL) {
    user_json_cjson = cJSON_Parse(user_json);
  }
  if (user_json_cjson == NULL) {
    user_json_cjson = cJSON_CreateObject();
  }
  bsp_error_t error = bsp_write_tensor_to_group_cjson(
      f, tensor, user_json_cjson, compression_level);
  cJSON_Delete(user_json_cjson);
  return error;
}

bsp_error_t bsp_write_tensor_cjson(const char* fname, bsp_tensor_t tensor,
                                   const char* group, cJSON* user_json,
                                   int compression_level) {
  if (group == NULL) {
    hid_t f = H5Fcreate(fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    bsp_error_t error =
        bsp_write_tensor_to_group_cjson(f, tensor, user_json, compression_level);
    if (error != BSP_SUCCESS) {
      H5Fclose(f);
      return error;
    }
    H5Fclose(f);
  } else {
    hid_t f;
    if (access(fname, F_OK) == 0) {
      f = H5Fopen(fname, H5F_ACC_RDWR, H5P_DEFAULT);
    } else {
      f = H5Fcreate(fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    }
    hid_t g = H5Gcreate1(f, group, H5P_DEFAULT);
    bsp_error_t error =
        bsp_write_tensor_to_group_cjson(g, tensor, user_json, compression_level);
    if (error != BSP_SUCCESS) {
      H5Gclose(g);
      H5Fclose(f);
      return error;
    }
    H5Gclose(g);
    H5Fclose(f);
  }
  return BSP_SUCCESS;
}

bsp_error_t bsp_write_tensor(const char* fname, bsp_tensor_t tensor,
                             const char* group, const char* user_json,
                             int compression_level) {
  cJSON* user_json_cjson = NULL;
  if (user_json != NULL) {
    user_json_cjson = cJSON_Parse(user_json);
  }
  if (user_json_cjson == NULL) {
    user_json_cjson = cJSON_CreateObject();
  }

  bsp_error_t error = bsp_write_tensor_cjson(
      fname, tensor, group, user_json_cjson, compression_level);
  cJSON_Delete(user_json_cjson);
  return error;
}
