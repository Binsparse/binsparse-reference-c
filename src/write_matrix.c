/*
 * SPDX-FileCopyrightText: 2024 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <binsparse/binsparse_all.h>
#include <binsparse/hdf5_wrapper.h>
#include <binsparse/matrix.h>
#include <cJSON/cJSON.h>
#include <unistd.h>

static void bsp_prepare_hdf5_runtime(void) {
  static bool initialized = false;
  if (!initialized) {
    H5dont_atexit();
    initialized = true;
  }
}

char* bsp_generate_json(bsp_matrix_t matrix, cJSON* user_json) {
  cJSON* j = cJSON_CreateObject();
  assert(j != NULL);

  cJSON* binsparse = cJSON_CreateObject();

  assert(binsparse != NULL);

  cJSON_AddItemToObject(j, "binsparse", binsparse);

  if (user_json != NULL) {
    cJSON* item;
    cJSON_ArrayForEach(item, user_json) {
      cJSON* item_copy = cJSON_Duplicate(item, 1); // 1 = deep copy
      cJSON_AddItemToObject(j, item->string, item_copy);
    }
  }

  cJSON_AddStringToObject(binsparse, "version", BINSPARSE_VERSION);

  cJSON_AddStringToObject(binsparse, "format",
                          bsp_get_matrix_format_string(matrix.format));

  cJSON* shape = cJSON_AddArrayToObject(binsparse, "shape");
  cJSON_AddItemToArray(shape, cJSON_CreateNumber(matrix.nrows));
  if (!bsp_matrix_format_is_vector(matrix.format)) {
    cJSON_AddItemToArray(shape, cJSON_CreateNumber(matrix.ncols));
  }

  cJSON_AddNumberToObject(binsparse, "number_of_stored_values", matrix.nnz);

  cJSON* data_types = cJSON_AddObjectToObject(binsparse, "data_types");

  if (!matrix.is_iso) {
    cJSON_AddStringToObject(data_types, "values",
                            bsp_get_type_string(matrix.values.type));
  } else {
    char* base_type_string = bsp_get_type_string(matrix.values.type);
    size_t len = strlen(base_type_string) + 6;
    char* type_string = (char*) malloc(sizeof(char) * len);

    strncpy(type_string, "iso[", len);
    strncpy(type_string + 4, base_type_string, len - 4);
    strncpy(type_string + len - 2, "]", 2);

    cJSON_AddStringToObject(data_types, "values", type_string);

    free(type_string);
  }

  if (matrix.indices_0.type != BSP_INVALID_TYPE) {
    cJSON_AddStringToObject(data_types, "indices_0",
                            bsp_get_type_string(matrix.indices_0.type));
  }

  if (matrix.indices_1.type != BSP_INVALID_TYPE) {
    cJSON_AddStringToObject(data_types, "indices_1",
                            bsp_get_type_string(matrix.indices_1.type));
  }

  if (matrix.pointers_to_1.type != BSP_INVALID_TYPE) {
    cJSON_AddStringToObject(data_types, "pointers_to_1",
                            bsp_get_type_string(matrix.pointers_to_1.type));
  }

  if (matrix.structure != BSP_GENERAL) {
    cJSON_AddStringToObject(binsparse, "structure",
                            bsp_get_structure_string(matrix.structure));
  }

  char* string = cJSON_Print(j);

  cJSON_Delete(j);

  return string;
}

bsp_error_t bsp_write_matrix_to_group_cjson(hid_t f, bsp_matrix_t matrix,
                                            cJSON* user_json,
                                            int compression_level) {
  if (bsp_matrix_format_is_vector(matrix.format) && matrix.ncols != 1) {
    return BSP_ERROR_INVALID_INPUT;
  }
  if (matrix.format == BSP_DVEC && matrix.nnz != matrix.nrows) {
    return BSP_ERROR_INVALID_INPUT;
  }

  bsp_error_t error =
      bsp_write_array(f, (char*) "values", matrix.values, compression_level);
  if (error != BSP_SUCCESS) {
    return error;
  }

  if (matrix.indices_0.type != BSP_INVALID_TYPE) {
    error = bsp_write_array(f, (char*) "indices_0", matrix.indices_0,
                            compression_level);
    if (error != BSP_SUCCESS) {
      return error;
    }
  }

  if (matrix.indices_1.type != BSP_INVALID_TYPE) {
    error = bsp_write_array(f, (char*) "indices_1", matrix.indices_1,
                            compression_level);
    if (error != BSP_SUCCESS) {
      return error;
    }
  }

  if (matrix.pointers_to_1.type != BSP_INVALID_TYPE) {
    error = bsp_write_array(f, (char*) "pointers_to_1", matrix.pointers_to_1,
                            compression_level);
    if (error != BSP_SUCCESS) {
      return error;
    }
  }

  char* json_string = bsp_generate_json(matrix, user_json);

  error = bsp_write_attribute(f, (char*) "binsparse", json_string);
  if (error != BSP_SUCCESS) {
    free(json_string);
    return error;
  }
  free(json_string);

  return BSP_SUCCESS;
}

bsp_error_t bsp_write_matrix_to_group(hid_t f, bsp_matrix_t matrix,
                                      const char* user_json,
                                      int compression_level) {
  cJSON* user_json_cjson = NULL;
  if (user_json != NULL) {
    user_json_cjson = cJSON_Parse(user_json);
  }
  if (user_json_cjson == NULL) {
    user_json_cjson = cJSON_CreateObject();
  }
  bsp_error_t error = bsp_write_matrix_to_group_cjson(
      f, matrix, user_json_cjson, compression_level);
  cJSON_Delete(user_json_cjson);
  return error;
}

bsp_error_t bsp_write_matrix_cjson(const char* fname, bsp_matrix_t matrix,
                                   const char* group, cJSON* user_json,
                                   int compression_level) {
  if (group == NULL) {
    bsp_prepare_hdf5_runtime();
    hid_t f = H5Fcreate(fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    bsp_error_t error = bsp_write_matrix_to_group_cjson(f, matrix, user_json,
                                                        compression_level);
    if (error != BSP_SUCCESS) {
      H5Fclose(f);
      return error;
    }
    H5Fclose(f);
  } else {
    hid_t f;
    bsp_prepare_hdf5_runtime();
    if (access(fname, F_OK) == 0) {
      f = H5Fopen(fname, H5F_ACC_RDWR, H5P_DEFAULT);
    } else {
      f = H5Fcreate(fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    }
    hid_t g = H5Gcreate1(f, group, H5P_DEFAULT);
    bsp_error_t error = bsp_write_matrix_to_group_cjson(g, matrix, user_json,
                                                        compression_level);
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

bsp_error_t bsp_write_matrix(const char* fname, bsp_matrix_t matrix,
                             const char* group, const char* user_json,
                             int compression_level) {
  cJSON* user_json_cjson = NULL;
  if (user_json != NULL) {
    user_json_cjson = cJSON_Parse(user_json);
  }
  if (user_json_cjson == NULL) {
    user_json_cjson = cJSON_CreateObject();
  }

  bsp_error_t error = bsp_write_matrix_cjson(
      fname, matrix, group, user_json_cjson, compression_level);
  cJSON_Delete(user_json_cjson);
  return error;
}
