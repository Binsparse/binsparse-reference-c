/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <binsparse/binsparse_all.h>
#include <stdio.h>
#include <unistd.h>

static void write_versioned_dvec(const char* filename, const char* version) {
  unlink(filename);

  bsp_matrix_t matrix;
  bsp_construct_default_matrix_t(&matrix);
  matrix.format = BSP_DVEC;
  matrix.structure = BSP_GENERAL;
  matrix.nrows = 2;
  matrix.ncols = 1;
  matrix.nnz = 2;
  assert(bsp_construct_array_t(&matrix.values, 2, BSP_FLOAT64) == BSP_SUCCESS);
  double* values = matrix.values.data;
  values[0] = 1;
  values[1] = 2;

  assert(bsp_write_matrix(filename, matrix, NULL, NULL, 0) == BSP_SUCCESS);
  bsp_destroy_matrix_t(&matrix);

  char json[1024];
  int length = snprintf(
      json, sizeof(json),
      "{\"binsparse\":{\"version\":\"%s\",\"format\":\"DVEC\","
      "\"shape\":[2],\"number_of_stored_values\":2,"
      "\"data_types\":{\"values\":\"float64\"}}}",
      version);
  assert(length > 0 && (size_t) length < sizeof(json));

  hid_t file = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT);
  assert(file >= 0);
  H5Adelete(file, "binsparse");
  assert(bsp_write_attribute(file, "binsparse", json) == BSP_SUCCESS);
  H5Fclose(file);
}

static void check_matrix_reader(const char* filename, const char* version,
                                bsp_error_t expected) {
  write_versioned_dvec(filename, version);

  bsp_matrix_t matrix;
  bsp_error_t error = bsp_read_matrix(&matrix, filename, NULL);
  assert(error == expected);
  if (error == BSP_SUCCESS) {
    bsp_destroy_matrix_t(&matrix);
  }

  unlink(filename);
}

int main(int argc, char** argv) {
  assert(argc == 2);
  const char* filename = argv[1];

  assert(bsp_check_version_compatible("0.1.0") == BSP_SUCCESS);
  assert(bsp_check_version_compatible("0.1.0+roundtrip.1") == BSP_SUCCESS);
  assert(bsp_check_version_compatible("0.1.1") == BSP_ERROR_UNSUPPORTED);
  assert(bsp_check_version_compatible("0.1.42+roundtrip.1") ==
         BSP_ERROR_UNSUPPORTED);
  assert(bsp_check_version_compatible("0.1.0-alpha.1") == BSP_SUCCESS);
  assert(bsp_check_version_compatible("0.2.0-alpha.1") ==
         BSP_ERROR_UNSUPPORTED);
  assert(bsp_check_version_compatible("0.2.0") == BSP_ERROR_UNSUPPORTED);
  assert(bsp_check_version_compatible("1.0.0") == BSP_ERROR_UNSUPPORTED);
  assert(bsp_check_version_compatible("not-a-version") == BSP_ERROR_FORMAT);
  assert(bsp_check_version_compatible(NULL) == BSP_ERROR_FORMAT);

  check_matrix_reader(filename, "0.1.0", BSP_SUCCESS);
  check_matrix_reader(filename, "0.1.0+roundtrip.1", BSP_SUCCESS);
  check_matrix_reader(filename, "0.1.7", BSP_ERROR_UNSUPPORTED);
  check_matrix_reader(filename, "0.1.0-alpha.1", BSP_SUCCESS);
  check_matrix_reader(filename, "0.2.0-alpha.1", BSP_ERROR_UNSUPPORTED);
  check_matrix_reader(filename, "0.2.0", BSP_ERROR_UNSUPPORTED);
  check_matrix_reader(filename, "1.0.0", BSP_ERROR_UNSUPPORTED);
  check_matrix_reader(filename, "not-a-version", BSP_ERROR_FORMAT);

  return 0;
}
