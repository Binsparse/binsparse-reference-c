/*
 * SPDX-FileCopyrightText: 2024 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <binsparse/binsparse_cJSON.h>
#include <binsparse/binsparse_hdf5.h>
#include <binsparse/detail/allocator.h>
#include <binsparse/hdf5_wrapper.h>
#include <binsparse/matrix.h>
#include <binsparse/matrix_market/matrix_market_read.h>
#include <binsparse/version.h>
#include <cJSON/cJSON.h>
#include <math.h>
#include <unistd.h>

static void bsp_prepare_hdf5_runtime(void) {
  static bool initialized = false;
  if (!initialized) {
    H5dont_atexit();
    initialized = true;
  }
}

static bool bsp_parse_dimension(const cJSON* item, size_t* dimension) {
  if (item == NULL || !cJSON_IsNumber(item)) {
    return false;
  }

  double value = cJSON_GetNumberValue(item);
  if (!isfinite(value) || value < 0 || floor(value) != value ||
      value >= (double) SIZE_MAX) {
    return false;
  }

  *dimension = (size_t) value;
  return true;
}

static bsp_error_t bsp_parse_matrix_shape(const cJSON* binsparse,
                                          bsp_matrix_format_t format,
                                          size_t* nrows, size_t* ncols) {
  cJSON* shape = cJSON_GetObjectItemCaseSensitive(binsparse, "shape");
  if (shape == NULL || !cJSON_IsArray(shape)) {
    return BSP_ERROR_FORMAT;
  }

  int expected_rank = bsp_matrix_format_is_vector(format) ? 1 : 2;
  if (cJSON_GetArraySize(shape) != expected_rank ||
      !bsp_parse_dimension(cJSON_GetArrayItem(shape, 0), nrows)) {
    return BSP_ERROR_FORMAT;
  }

  if (expected_rank == 1) {
    *ncols = 1;
  } else if (!bsp_parse_dimension(cJSON_GetArrayItem(shape, 1), ncols)) {
    return BSP_ERROR_FORMAT;
  }

  return BSP_SUCCESS;
}

#if __STDC_VERSION__ >= 201112L
bsp_error_t bsp_read_matrix_from_group_parallel(bsp_matrix_t* matrix, hid_t f,
                                                int num_threads) {
  if (f < 0) {
    return BSP_ERROR_IO;
  }

  bsp_construct_default_matrix_t(matrix);

  char* json_string;
  bsp_error_t error = bsp_read_attribute(&json_string, f, (char*) "binsparse");
  if (error != BSP_SUCCESS) {
    return error;
  }

  cJSON* j = cJSON_Parse(json_string);

  if (j == NULL || !cJSON_IsObject(j)) {
    return BSP_ERROR_FORMAT;
  }

  cJSON* binsparse = cJSON_GetObjectItemCaseSensitive(j, "binsparse");
  assert(cJSON_IsObject(binsparse));

  cJSON* version_ = cJSON_GetObjectItemCaseSensitive(binsparse, "version");

  assert(version_ != NULL);

  assert(cJSON_IsString(version_));

  error = bsp_check_version_compatible(cJSON_GetStringValue(version_));
  if (error != BSP_SUCCESS) {
    cJSON_Delete(j);
    free(json_string);
    return error;
  }

  cJSON* format_ = cJSON_GetObjectItemCaseSensitive(binsparse, "format");
  assert(format_ != NULL);
  char* format_string = cJSON_GetStringValue(format_);

  bsp_matrix_format_t format = bsp_get_matrix_format(format_string);

  assert(format != 0);

  matrix->format = format;

  cJSON* nnz_ =
      cJSON_GetObjectItemCaseSensitive(binsparse, "number_of_stored_values");
  assert(nnz_ != NULL);
  size_t nnz = cJSON_GetNumberValue(nnz_);

  size_t nrows;
  size_t ncols;
  error = bsp_parse_matrix_shape(binsparse, format, &nrows, &ncols);
  if (error != BSP_SUCCESS || (format == BSP_DVEC && nnz != nrows)) {
    cJSON_Delete(j);
    free(json_string);
    return BSP_ERROR_FORMAT;
  }

  matrix->nrows = nrows;
  matrix->ncols = ncols;
  matrix->nnz = nnz;
  matrix->format = format;

  cJSON* data_types_ =
      cJSON_GetObjectItemCaseSensitive(binsparse, "data_types");
  assert(data_types_ != NULL);

  if (cJSON_HasObjectItem(data_types_, "values")) {
    error = bsp_read_array_parallel(&matrix->values, f, (char*) "values",
                                    num_threads);
    if (error != BSP_SUCCESS) {
      free(json_string);
      return error;
    }

    cJSON* value_type = cJSON_GetObjectItemCaseSensitive(data_types_, "values");
    char* type_string = cJSON_GetStringValue(value_type);

    if (strlen(type_string) >= 4 && strncmp(type_string, "iso[", 4) == 0) {
      matrix->is_iso = true;
      type_string += 4;
    }

    if (strlen(type_string) >= 8 && strncmp(type_string, "complex[", 8) == 0) {
      bsp_error_t error = bsp_fp_array_to_complex(&matrix->values);
      if (error != BSP_SUCCESS) {
        // TODO: handle error
        return error;
      }
    } else if (strncmp(type_string, "bint8", 5) == 0) {
      matrix->values.type = BSP_BINT8;
    }
  }

  if (cJSON_HasObjectItem(data_types_, "indices_0")) {
    error = bsp_read_array_parallel(&matrix->indices_0, f, (char*) "indices_0",
                                    num_threads);
    if (error != BSP_SUCCESS) {
      free(json_string);
      bsp_destroy_array_t(&matrix->values);
      return error;
    }
  }

  if (cJSON_HasObjectItem(data_types_, "indices_1")) {
    error = bsp_read_array_parallel(&matrix->indices_1, f, (char*) "indices_1",
                                    num_threads);
    if (error != BSP_SUCCESS) {
      free(json_string);
      bsp_destroy_array_t(&matrix->values);
      bsp_destroy_array_t(&matrix->indices_0);
      return error;
    }
  }

  if (cJSON_HasObjectItem(data_types_, "pointers_to_1")) {
    error = bsp_read_array_parallel(&matrix->pointers_to_1, f,
                                    (char*) "pointers_to_1", num_threads);
    if (error != BSP_SUCCESS) {
      free(json_string);
      bsp_destroy_array_t(&matrix->values);
      bsp_destroy_array_t(&matrix->indices_0);
      bsp_destroy_array_t(&matrix->indices_1);
      return error;
    }
  }

  if (cJSON_HasObjectItem(binsparse, "structure")) {
    cJSON* structure_ =
        cJSON_GetObjectItemCaseSensitive(binsparse, "structure");
    char* structure = cJSON_GetStringValue(structure_);
    matrix->structure = bsp_get_structure(structure);
  }

  cJSON_Delete(j);
  free(json_string);

  return BSP_SUCCESS;
}
#endif

bsp_error_t bsp_read_matrix_from_group_allocator(bsp_matrix_t* matrix, hid_t f,
                                                 bsp_allocator_t allocator) {
  if (f < 0) {
    return BSP_ERROR_IO;
  }

  bsp_construct_default_matrix_t_allocator(matrix, allocator);

  char* json_string;
  bsp_error_t error = bsp_read_attribute_allocator(
      &json_string, f, (char*) "binsparse", allocator);
  if (error != BSP_SUCCESS) {
    return error;
  }

  cJSON* j = cJSON_Parse(json_string);

  if (j == NULL || !cJSON_IsObject(j)) {
    allocator.free(json_string);
    return BSP_ERROR_FORMAT;
  }

  cJSON* binsparse = cJSON_GetObjectItemCaseSensitive(j, "binsparse");
  if (!cJSON_IsObject(binsparse)) {
    cJSON_Delete(j);
    allocator.free(json_string);
    return BSP_ERROR_FORMAT;
  }

  cJSON* version_ = cJSON_GetObjectItemCaseSensitive(binsparse, "version");

  if (version_ == NULL || !cJSON_IsString(version_)) {
    cJSON_Delete(j);
    allocator.free(json_string);
    return BSP_ERROR_FORMAT;
  }

  error = bsp_check_version_compatible(cJSON_GetStringValue(version_));
  if (error != BSP_SUCCESS) {
    cJSON_Delete(j);
    allocator.free(json_string);
    return error;
  }

  cJSON* format_ = cJSON_GetObjectItemCaseSensitive(binsparse, "format");
  if (format_ == NULL || !cJSON_IsString(format_)) {
    cJSON_Delete(j);
    allocator.free(json_string);
    return BSP_ERROR_FORMAT;
  }
  char* format_string = cJSON_GetStringValue(format_);

  bsp_matrix_format_t format = bsp_get_matrix_format(format_string);

  if (format == BSP_INVALID_FORMAT) {
    cJSON_Delete(j);
    allocator.free(json_string);
    return BSP_ERROR_FORMAT;
  }

  matrix->format = format;

  cJSON* nnz_ =
      cJSON_GetObjectItemCaseSensitive(binsparse, "number_of_stored_values");
  if (nnz_ == NULL || !cJSON_IsNumber(nnz_)) {
    cJSON_Delete(j);
    allocator.free(json_string);
    return BSP_ERROR_FORMAT;
  }
  size_t nnz = cJSON_GetNumberValue(nnz_);

  size_t nrows;
  size_t ncols;
  error = bsp_parse_matrix_shape(binsparse, format, &nrows, &ncols);
  if (error != BSP_SUCCESS || (format == BSP_DVEC && nnz != nrows)) {
    cJSON_Delete(j);
    allocator.free(json_string);
    return BSP_ERROR_FORMAT;
  }

  matrix->nrows = nrows;
  matrix->ncols = ncols;
  matrix->nnz = nnz;
  matrix->format = format;

  cJSON* data_types_ =
      cJSON_GetObjectItemCaseSensitive(binsparse, "data_types");
  if (data_types_ == NULL || !cJSON_IsObject(data_types_)) {
    cJSON_Delete(j);
    allocator.free(json_string);
    return BSP_ERROR_FORMAT;
  }

  if (cJSON_HasObjectItem(data_types_, "values")) {
    error = bsp_read_array_allocator(&matrix->values, f, (char*) "values",
                                     allocator);
    if (error != BSP_SUCCESS) {
      allocator.free(json_string);
      return error;
    }

    cJSON* value_type = cJSON_GetObjectItemCaseSensitive(data_types_, "values");
    char* type_string = cJSON_GetStringValue(value_type);

    if (strlen(type_string) >= 4 && strncmp(type_string, "iso[", 4) == 0) {
      matrix->is_iso = true;
      type_string += 4;
    }

    if (strlen(type_string) >= 8 && strncmp(type_string, "complex[", 8) == 0) {
      bsp_error_t error = bsp_fp_array_to_complex(&matrix->values);
      if (error != BSP_SUCCESS) {
        // TODO: handle error
        return error;
      }
    } else if (strncmp(type_string, "bint8", 5) == 0) {
      matrix->values.type = BSP_BINT8;
    }
  }

  if (cJSON_HasObjectItem(data_types_, "indices_0")) {
    error = bsp_read_array_allocator(&matrix->indices_0, f, (char*) "indices_0",
                                     allocator);
    if (error != BSP_SUCCESS) {
      allocator.free(json_string);
      bsp_destroy_array_t(&matrix->values);
      return error;
    }
  }

  if (cJSON_HasObjectItem(data_types_, "indices_1")) {
    error = bsp_read_array_allocator(&matrix->indices_1, f, (char*) "indices_1",
                                     allocator);
    if (error != BSP_SUCCESS) {
      allocator.free(json_string);
      bsp_destroy_array_t(&matrix->values);
      bsp_destroy_array_t(&matrix->indices_0);
      return error;
    }
  }

  if (cJSON_HasObjectItem(data_types_, "pointers_to_1")) {
    error = bsp_read_array_allocator(&matrix->pointers_to_1, f,
                                     (char*) "pointers_to_1", allocator);
    if (error != BSP_SUCCESS) {
      allocator.free(json_string);
      bsp_destroy_array_t(&matrix->values);
      bsp_destroy_array_t(&matrix->indices_0);
      bsp_destroy_array_t(&matrix->indices_1);
      return error;
    }
  }

  if (cJSON_HasObjectItem(binsparse, "structure")) {
    cJSON* structure_ =
        cJSON_GetObjectItemCaseSensitive(binsparse, "structure");
    char* structure = cJSON_GetStringValue(structure_);
    matrix->structure = bsp_get_structure(structure);
  }

  cJSON_Delete(j);
  allocator.free(json_string);

  return BSP_SUCCESS;
}

bsp_error_t bsp_read_matrix_from_group(bsp_matrix_t* matrix, hid_t f) {
  return bsp_read_matrix_from_group_allocator(matrix, f, bsp_default_allocator);
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

#if __STDC_VERSION__ >= 201112L
bsp_error_t bsp_read_matrix_parallel(bsp_matrix_t* matrix,
                                     const char* file_name, const char* group,
                                     int num_threads) {
  if (group == NULL) {
    size_t idx = bsp_final_dot(file_name);
    if (strcmp(file_name + idx, ".hdf5") == 0 ||
        strcmp(file_name + idx, ".h5") == 0) {
      if (access(file_name, F_OK) != 0) {
        return BSP_ERROR_IO;
      }
      bsp_prepare_hdf5_runtime();
      hid_t f = H5Fopen(file_name, H5F_ACC_RDONLY, H5P_DEFAULT);
      if (f < 0) {
        return BSP_ERROR_IO;
      }
      bsp_error_t error =
          bsp_read_matrix_from_group_parallel(matrix, f, num_threads);
      H5Fclose(f);
      return error;
    } else if (strcmp(file_name + idx, ".mtx") == 0) {
      // TODO: implement error-handling for Matrix Market.
      *matrix = bsp_mmread(file_name);
      return BSP_SUCCESS;
    } else {
      return BSP_ERROR_IO;
    }
  } else {
    if (access(file_name, F_OK) != 0) {
      return BSP_ERROR_IO;
    }
    bsp_prepare_hdf5_runtime();
    hid_t f = H5Fopen(file_name, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (f < 0) {
      return BSP_ERROR_IO;
    }
    hid_t g = H5Gopen1(f, group);
    if (g < 0) {
      H5Fclose(f);
      return BSP_ERROR_IO;
    }
    bsp_error_t error =
        bsp_read_matrix_from_group_parallel(matrix, g, num_threads);
    H5Gclose(g);
    H5Fclose(f);
    return error;
  }
}
#endif

bsp_error_t bsp_read_matrix_allocator(bsp_matrix_t* matrix,
                                      const char* file_name, const char* group,
                                      bsp_allocator_t allocator) {
  if (group == NULL) {
    size_t idx = bsp_final_dot(file_name);
    if (strcmp(file_name + idx, ".hdf5") == 0 ||
        strcmp(file_name + idx, ".h5") == 0) {
      if (access(file_name, F_OK) != 0) {
        return BSP_ERROR_IO;
      }
      bsp_prepare_hdf5_runtime();
      hid_t f = H5Fopen(file_name, H5F_ACC_RDONLY, H5P_DEFAULT);
      if (f < 0) {
        return BSP_ERROR_IO;
      }
      bsp_error_t error =
          bsp_read_matrix_from_group_allocator(matrix, f, allocator);
      H5Fclose(f);
      return error;
    } else if (strcmp(file_name + idx, ".mtx") == 0) {
      // TODO: implement error-handling for Matrix Market.
      *matrix = bsp_mmread(file_name);
      return BSP_SUCCESS;
    } else {
      return BSP_ERROR_IO;
    }
  } else {
    if (access(file_name, F_OK) != 0) {
      return BSP_ERROR_IO;
    }
    bsp_prepare_hdf5_runtime();
    hid_t f = H5Fopen(file_name, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (f < 0) {
      return BSP_ERROR_IO;
    }
    hid_t g = H5Gopen1(f, group);
    if (g < 0) {
      H5Fclose(f);
      return BSP_ERROR_IO;
    }
    bsp_error_t error =
        bsp_read_matrix_from_group_allocator(matrix, g, allocator);
    H5Gclose(g);
    H5Fclose(f);
    return error;
  }
}

bsp_error_t bsp_read_matrix(bsp_matrix_t* matrix, const char* file_name,
                            const char* group) {
  return bsp_read_matrix_allocator(matrix, file_name, group,
                                   bsp_default_allocator);
}
