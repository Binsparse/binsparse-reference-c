#include "reformat.h"
#include "cnpy_c.h"

#include <binsparse/hdf5_wrapper.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t shape_size(const size_t *shape, int rank) {
  size_t size = 1;
  for (int d = 0; d < rank; ++d) size *= shape[d];
  return size;
}

int main(int argc, char **argv) {
  cJSON *header;
  const char *format;
  bool custom;
  bsp_tensor_t tensor = bsp_construct_default_tensor_t();
  bsp_matrix_t matrix;
  bsp_array_t values, fill_value;
  entries_t entries;
  size_t *shape, *transpose, size;
  void *dense;
  uint8_t *pattern;
  bool owns_fill = false;

  if (argc != 5) {
    fprintf(stderr, "usage: binsparse_to_npy <tensor_in> <tensor_out> "
                    "<pattern_out> <fill_value_out>\n");
    return 2;
  }
  header = input_header(argv[1]);
  format = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(header, "format"));
  if (format == NULL) die("input header has no format");
  custom = !strcmp(format, "custom");
  int rank;
  if (custom) {
    tensor = bsp_read_tensor(argv[1], NULL);
    tensor.is_iso = header_values_are_iso(header);
    rank = tensor.rank;
    shape = tensor.dims;
    values = bsp_get_tensor_values(tensor);
    entries = tensor_to_coo(&tensor);
  } else {
    if (bsp_read_matrix(&matrix, argv[1], NULL) != BSP_SUCCESS)
      die("cannot read Binsparse matrix");
    rank = bsp_matrix_format_is_vector(matrix.format) ? 1 : 2;
    shape = malloc((size_t) rank * sizeof(size_t));
    shape[0] = matrix.nrows;
    if (rank == 2) shape[1] = matrix.ncols;
    values = matrix.values;
    entries = matrix_to_coo(&matrix);
  }
  transpose = rank ? malloc((size_t) rank * sizeof(size_t)) : NULL;
  for (int d = 0; d < rank; ++d) transpose[d] = (size_t) d;
  if (custom && tensor.transpose)
    memcpy(transpose, tensor.transpose, (size_t) rank * sizeof(size_t));
  else if (!custom && rank == 2 &&
           (matrix.format == BSP_DMATC || matrix.format == BSP_CSC ||
            matrix.format == BSP_DCSC || matrix.format == BSP_COOC)) {
    transpose[0] = 1; transpose[1] = 0;
  }

  bsp_construct_default_array_t(&fill_value);
  cJSON *fill = cJSON_GetObjectItemCaseSensitive(header, "fill");
  if (cJSON_IsTrue(fill)) {
    hid_t file = H5Fopen(argv[1], H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file < 0 || bsp_read_array(&fill_value, file, "fill_value") != BSP_SUCCESS)
      die("cannot read fill_value");
    H5Fclose(file);
    const char *fill_type = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(header, "data_types"), "fill_value"));
    if (fill_type != NULL && strncmp(fill_type, "complex[", 8) == 0 &&
        bsp_fp_array_to_complex(&fill_value) != BSP_SUCCESS)
      die("invalid complex fill_value");
    if (fill_type != NULL && strncmp(fill_type, "bint8", 5) == 0)
      fill_value.type = BSP_BINT8;
    owns_fill = true;
  } else {
    if (bsp_construct_array_t(&fill_value, 1, values.type) != BSP_SUCCESS)
      die("out of memory");
    memset(fill_value.data, 0, bsp_type_size(values.type));
    owns_fill = true;
  }
  size = shape_size(shape, rank);
  dense = malloc(size * bsp_type_size(values.type));
  pattern = calloc(size, 1);
  if ((size && !dense) || (size && !pattern)) die("out of memory");
  for (size_t i = 0; i < size; ++i)
    memcpy((char *) dense + i * bsp_type_size(values.type), fill_value.data,
           bsp_type_size(values.type));
  for (size_t k = 0; k < entries.size; ++k) {
    size_t linear = 0;
    for (int logical = 0; logical < rank; ++logical) {
      int stored = 0;
      while (stored < rank && transpose[stored] != (size_t) logical) ++stored;
      linear = linear * shape[logical] + entries.entry[k].coord[stored];
    }
    pattern[linear] = 1;
    memcpy((char *) dense + linear * bsp_type_size(values.type),
           (char *) values.data + entries.entry[k].value * bsp_type_size(values.type),
           bsp_type_size(values.type));
  }
  if (cnpy_c_save(argv[2], dense, shape, (size_t) rank, values.type) ||
      cnpy_c_save(argv[3], pattern, shape, (size_t) rank, BSP_BINT8) ||
      cnpy_c_save(argv[4], fill_value.data, NULL, 0, fill_value.type))
    die("cannot write NPY output");

  free(dense); free(pattern); free(transpose); free_entries(&entries);
  if (owns_fill) bsp_destroy_array_t(&fill_value);
  if (custom) bsp_destroy_tensor_t(tensor);
  else { free(shape); bsp_destroy_matrix_t(&matrix); }
  cJSON_Delete(header);
  return 0;
}
