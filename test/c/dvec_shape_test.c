/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <binsparse/binsparse_all.h>
#include <cJSON/cJSON.h>
#include <stdio.h>
#include <unistd.h>

static void check_shape(const char* filename) {
  hid_t file = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
  assert(file >= 0);

  char* json_string = NULL;
  assert(bsp_read_attribute(&json_string, file, "binsparse") == BSP_SUCCESS);
  H5Fclose(file);

  cJSON* json = cJSON_Parse(json_string);
  assert(json != NULL);
  cJSON* binsparse = cJSON_GetObjectItemCaseSensitive(json, "binsparse");
  assert(cJSON_IsObject(binsparse));
  cJSON* shape = cJSON_GetObjectItemCaseSensitive(binsparse, "shape");
  assert(cJSON_IsArray(shape));
  assert(cJSON_GetArraySize(shape) == 1);
  cJSON* dimension = cJSON_GetArrayItem(shape, 0);
  assert(cJSON_IsNumber(dimension));
  assert(cJSON_GetNumberValue(dimension) == 3);

  cJSON_Delete(json);
  free(json_string);
}

static void check_read(const char* filename, bool parallel) {
  bsp_matrix_t matrix;
  bsp_error_t error;
  if (parallel) {
    error = bsp_read_matrix_parallel(&matrix, filename, NULL, 2);
  } else {
    error = bsp_read_matrix(&matrix, filename, NULL);
  }
  assert(error == BSP_SUCCESS);
  assert(matrix.format == BSP_DVEC);
  assert(matrix.nrows == 3);
  assert(matrix.ncols == 1);
  assert(matrix.nnz == 3);
  assert(matrix.values.size == 3);
  bsp_destroy_matrix_t(&matrix);
}

static void create_invalid_shape(const char* filename) {
  static const char* json =
      "{\"binsparse\":{\"version\":\"0.1.0\",\"format\":\"DVEC\","
      "\"shape\":[3,1],\"number_of_stored_values\":3,"
      "\"data_types\":{\"values\":\"float64\"}}}";

  hid_t file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  assert(file >= 0);
  assert(bsp_write_attribute(file, "binsparse", json) == BSP_SUCCESS);
  H5Fclose(file);
}

int main(int argc, char** argv) {
  assert(argc == 2);
  const char* filename = argv[1];
  char invalid_filename[4096];
  int length = snprintf(invalid_filename, sizeof(invalid_filename),
                        "%s.invalid.h5", filename);
  assert(length > 0 && (size_t) length < sizeof(invalid_filename));
  unlink(filename);
  unlink(invalid_filename);

  bsp_matrix_t matrix;
  bsp_construct_default_matrix_t(&matrix);
  matrix.format = BSP_DVEC;
  matrix.structure = BSP_GENERAL;
  matrix.nrows = 3;
  matrix.ncols = 1;
  matrix.nnz = 3;
  assert(bsp_construct_array_t(&matrix.values, 3, BSP_FLOAT64) == BSP_SUCCESS);
  double* values = matrix.values.data;
  values[0] = 1;
  values[1] = 2;
  values[2] = 3;

  assert(bsp_write_matrix(filename, matrix, NULL, NULL, 0) == BSP_SUCCESS);
  check_shape(filename);
  check_read(filename, false);
  check_read(filename, true);

  matrix.ncols = 2;
  assert(bsp_write_matrix(invalid_filename, matrix, NULL, NULL, 0) ==
         BSP_ERROR_INVALID_INPUT);
  matrix.ncols = 1;
  unlink(invalid_filename);

  create_invalid_shape(invalid_filename);
  bsp_matrix_t invalid;
  assert(bsp_read_matrix(&invalid, invalid_filename, NULL) == BSP_ERROR_FORMAT);
  assert(bsp_read_matrix_parallel(&invalid, invalid_filename, NULL, 2) ==
         BSP_ERROR_FORMAT);

  unlink(filename);
  unlink(invalid_filename);
  bsp_destroy_matrix_t(&matrix);
  return 0;
}
