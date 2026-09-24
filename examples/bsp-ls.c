/*
 * SPDX-FileCopyrightText: 2024 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <binsparse/binsparse_all.h>

herr_t visit_group(hid_t loc_id, const char* name, const H5L_info_t* linfo,
                   void* opdata);

void print_group_info(hid_t g, const char* name) {
  hid_t bsp_json;

  H5E_BEGIN_TRY {
    bsp_json = H5Aopen(g, "binsparse", H5P_DEFAULT);
  }
  H5E_END_TRY;

  if (bsp_json != H5I_INVALID_HID) {
    char* json_string;
    bsp_error_t error = bsp_read_attribute(&json_string, g, "binsparse");
    if (error != BSP_SUCCESS) {
      printf("Error reading binsparse attribute: %s\n",
             bsp_get_error_string(error));
      return;
    }

    cJSON* j = cJSON_Parse(json_string);

    assert(j != NULL);
    assert(cJSON_IsObject(j));

    cJSON* binsparse = cJSON_GetObjectItemCaseSensitive(j, "binsparse");
    assert(cJSON_IsObject(binsparse));

    cJSON* version_ = cJSON_GetObjectItemCaseSensitive(binsparse, "version");

    assert(version_ != NULL);

    assert(cJSON_IsString(version_));
    char* version_string = cJSON_GetStringValue(version_);

    error = bsp_check_version_compatible(version_string);
    if (error != BSP_SUCCESS) {
      printf("Unsupported Binsparse version %s: %s\n", version_string,
             bsp_get_error_string(error));
      cJSON_Delete(j);
      free(json_string);
      return;
    }

    cJSON* format_ = cJSON_GetObjectItemCaseSensitive(binsparse, "format");
    assert(format_ != NULL);
    char* format_string = cJSON_GetStringValue(format_);

    cJSON* nnz_ =
        cJSON_GetObjectItemCaseSensitive(binsparse, "number_of_stored_values");
    assert(nnz_ != NULL);
    size_t nnz = cJSON_GetNumberValue(nnz_);

    cJSON* shape_ = cJSON_GetObjectItemCaseSensitive(binsparse, "shape");
    assert(shape_ != NULL);

    bool is_vector =
        bsp_matrix_format_is_vector(bsp_get_matrix_format(format_string));
    assert(cJSON_GetArraySize(shape_) == (is_vector ? 1 : 2));

    cJSON* nrows_ = cJSON_GetArrayItem(shape_, 0);
    assert(nrows_ != NULL);

    size_t nrows = cJSON_GetNumberValue(nrows_);

    char full_group_path[2048];
    size_t size = H5Iget_name(g, full_group_path, 2048);

    if (is_vector) {
      printf("Group \"%s\": Version %s Binsparse vector. Format %s, length "
             "%zu. %zu stored values.\n",
             full_group_path, version_string, format_string, nrows, nnz);
    } else {
      cJSON* ncols_ = cJSON_GetArrayItem(shape_, 1);
      assert(ncols_ != NULL);
      size_t ncols = cJSON_GetNumberValue(ncols_);
      printf("Group \"%s\": Version %s Binsparse matrix. Format %s, %zu x "
             "%zu. %zu stored values.\n",
             full_group_path, version_string, format_string, nrows, ncols, nnz);
    }

    cJSON* data_types =
        cJSON_GetObjectItemCaseSensitive(binsparse, "data_types");
    assert(data_types != NULL);

    cJSON* item;
    cJSON_ArrayForEach(item, data_types) {
      printf("  %s: %s\n", item->string, cJSON_Print(item));
    }
  }

  H5Literate(g, H5_INDEX_NAME, H5_ITER_INC, NULL, visit_group, NULL);
}

herr_t visit_group(hid_t loc_id, const char* name, const H5L_info_t* linfo,
                   void* opdata) {
  hid_t g = H5Oopen(loc_id, name, H5P_DEFAULT);
  H5I_type_t type = H5Iget_type(g);
  if (type == H5I_GROUP) {
    print_group_info(g, name);
  }
  H5Oclose(g);
  return 0;
}

int main(int argc, char** argv) {

  if (argc < 2) {
    printf("usage: ./bsp-ls [file_name.mtx]\n");
    return 1;
  }

  char* fname = argv[1];

  hid_t f = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);

  print_group_info(f, "/");

  H5Fclose(f);

  return 0;
}
