/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Reformat a Binsparse matrix or tensor through a coordinate list.
 *
 * Usage: reformat INPUT OUTPUT TARGET_HEADER.json [COMPRESSION]
 *
 * INPUT may use a predefined matrix/vector alias or the custom tensor format.
 * TARGET_HEADER is a Binsparse header (the object containing "format", not the
 * outer {"binsparse": ...} wrapper).  Both predefined aliases and arbitrary
 * custom dense/sparse level trees are accepted.
 */

#include <binsparse/matrix_formats.h>
#include <binsparse/read_matrix.h>
#include <binsparse/read_tensor.h>
#include <binsparse/write_matrix.h>
#include <binsparse/write_tensor.h>
#include <cJSON/cJSON.h>
#include <hdf5.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bsp_error_t bsp_read_attribute(char **attribute, hid_t f, char *name);

typedef struct {
  size_t *coord;
  size_t value;
} entry_t;

typedef struct {
  entry_t *entry;
  size_t size;
  size_t capacity;
  int rank;
} entries_t;

static void die(const char *message) {
  fprintf(stderr, "reformat: %s\n", message);
  exit(EXIT_FAILURE);
}

static cJSON *read_json(const char *path) {
  FILE *file = fopen(path, "rb");
  long size = 0;
  char *text;
  cJSON *json;
  if (file == NULL) die(strerror(errno));
  if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
      fseek(file, 0, SEEK_SET) != 0)
    die("cannot read target header");
  text = malloc((size_t) size + 1);
  if (text == NULL) die("out of memory");
  if (fread(text, 1, (size_t) size, file) != (size_t) size)
    die("cannot read target header");
  fclose(file);
  text[size] = '\0';
  json = cJSON_Parse(text);
  free(text);
  if (!cJSON_IsObject(json)) die("target header must be a JSON object");
  return json;
}

static cJSON *input_header(const char *path) {
  hid_t file = H5Fopen(path, H5F_ACC_RDONLY, H5P_DEFAULT);
  char *text = NULL;
  cJSON *outer, *header;
  if (file < 0 || bsp_read_attribute(&text, file, "binsparse") != BSP_SUCCESS)
    die("cannot read input Binsparse header");
  H5Fclose(file);
  outer = cJSON_Parse(text);
  free(text);
  if (!cJSON_IsObject(outer)) die("invalid input Binsparse header");
  header = cJSON_DetachItemFromObjectCaseSensitive(outer, "binsparse");
  cJSON_Delete(outer);
  if (!cJSON_IsObject(header)) die("input has no binsparse header");
  return header;
}

static bool header_values_are_iso(cJSON *header) {
  cJSON *types = cJSON_GetObjectItemCaseSensitive(header, "data_types");
  const char *value = cJSON_GetStringValue(
      cJSON_GetObjectItemCaseSensitive(types, "values"));
  return value != NULL && strncmp(value, "iso[", 4) == 0;
}

static void push_entry(entries_t *entries, const size_t *coord, size_t value) {
  entry_t *item;
  if (entries->size == entries->capacity) {
    size_t capacity = entries->capacity == 0 ? 16 : 2 * entries->capacity;
    entry_t *grown = realloc(entries->entry, capacity * sizeof(*grown));
    if (grown == NULL) die("out of memory");
    entries->entry = grown;
    entries->capacity = capacity;
  }
  item = &entries->entry[entries->size++];
  item->coord = malloc((size_t) entries->rank * sizeof(*item->coord));
  if (item->coord == NULL) die("out of memory");
  memcpy(item->coord, coord, (size_t) entries->rank * sizeof(*coord));
  item->value = value;
}

static void free_entries(entries_t *entries) {
  for (size_t i = 0; i < entries->size; ++i) free(entries->entry[i].coord);
  free(entries->entry);
}

/* Expand one custom level.  begin/end identify the parent positions entering
 * the level; coordinates are stored row-wise in coords. */
static void level_to_coo(const bsp_level_t *level, const bsp_tensor_t *tensor,
                         size_t depth, size_t parents, const size_t *coords,
                         entries_t *out) {
  if (level->kind == BSP_TENSOR_ELEMENT) {
    const bsp_element_t *element = level->data;
    if (parents != tensor->nnz)
      die("element positions do not match number_of_stored_values");
    for (size_t p = 0; p < parents; ++p)
      push_entry(out, coords + p * (size_t) tensor->rank,
                 tensor->is_iso ? 0 : p);
    (void) element;
    return;
  }

  if (level->kind == BSP_TENSOR_DENSE) {
    const bsp_dense_t *dense = level->data;
    size_t width = 1, count, *next;
    for (int d = 0; d < dense->rank; ++d)
      width *= tensor->dims[depth + (size_t) d];
    count = parents * width;
    next = calloc(count * (size_t) tensor->rank, sizeof(*next));
    if (next == NULL && count != 0) die("out of memory");
    for (size_t p = 0; p < parents; ++p) {
      for (size_t suffix = 0; suffix < width; ++suffix) {
        size_t q = p * width + suffix, n = suffix;
        memcpy(next + q * (size_t) tensor->rank,
               coords + p * (size_t) tensor->rank,
               depth * sizeof(*next));
        for (int d = dense->rank - 1; d >= 0; --d) {
          size_t dim = tensor->dims[depth + (size_t) d];
          next[q * (size_t) tensor->rank + depth + (size_t) d] = n % dim;
          n /= dim;
        }
      }
    }
    level_to_coo(dense->child, tensor, depth + (size_t) dense->rank, count,
                 next, out);
    free(next);
    return;
  }

  if (level->kind == BSP_TENSOR_SPARSE) {
    const bsp_sparse_t *sparse = level->data;
    size_t count = sparse->indices[0].size, *next;
    size_t q = 0;
    next = calloc(count * (size_t) tensor->rank, sizeof(*next));
    if (next == NULL && count != 0) die("out of memory");
    for (size_t p = 0; p < parents; ++p) {
      size_t begin = 0, end = count;
      if (sparse->pointers_to != NULL) {
        bsp_array_t pointers = *sparse->pointers_to;
        bsp_array_read(pointers, p, begin);
        bsp_array_read(pointers, p + 1, end);
      }
      for (size_t i = begin; i < end; ++i, ++q) {
        memcpy(next + q * (size_t) tensor->rank,
               coords + p * (size_t) tensor->rank,
               depth * sizeof(*next));
        for (int d = 0; d < sparse->rank; ++d) {
          size_t index = 0;
          bsp_array_read(sparse->indices[d], i, index);
          if (index >= tensor->dims[depth + (size_t) d])
            die("input coordinate is out of bounds");
          next[q * (size_t) tensor->rank + depth + (size_t) d] = index;
        }
      }
    }
    if (q != count) die("invalid sparse pointers");
    level_to_coo(sparse->child, tensor, depth + (size_t) sparse->rank, count,
                 next, out);
    free(next);
    return;
  }
  die("unknown tensor level");
}

static entries_t tensor_to_coo(const bsp_tensor_t *tensor) {
  entries_t result = {0};
  size_t *root;
  result.rank = tensor->rank;
  root = calloc((size_t) tensor->rank, sizeof(*root));
  if (root == NULL) die("out of memory");
  level_to_coo(tensor->level, tensor, 0, 1, root, &result);
  free(root);
  return result;
}

static entries_t matrix_to_coo(bsp_matrix_t *matrix) {
  entries_t result = {0};
  result.rank = bsp_matrix_format_is_vector(matrix->format) ? 1 : 2;
  if (matrix->format == BSP_COOR || matrix->format == BSP_COOC ||
      matrix->format == BSP_CVEC) {
    for (size_t i = 0; i < matrix->nnz; ++i) {
      size_t coord[2] = {0, 0};
      bsp_array_read(matrix->indices_0, i, coord[0]);
      if (result.rank == 2) bsp_array_read(matrix->indices_1, i, coord[1]);
      push_entry(&result, coord, matrix->is_iso ? 0 : i);
    }
    return result;
  }
  if (matrix->format == BSP_CSR || matrix->format == BSP_CSC) {
    size_t fibers = matrix->format == BSP_CSR ? matrix->nrows : matrix->ncols;
    for (size_t p = 0; p < fibers; ++p) {
      size_t begin = 0, end = 0;
      bsp_array_read(matrix->pointers_to_1, p, begin);
      bsp_array_read(matrix->pointers_to_1, p + 1, end);
      for (size_t i = begin; i < end; ++i) {
        size_t coord[2] = {p, 0};
        bsp_array_read(matrix->indices_1, i, coord[1]);
        push_entry(&result, coord, matrix->is_iso ? 0 : i);
      }
    }
    return result;
  }
  if (matrix->format == BSP_DCSR || matrix->format == BSP_DCSC) {
    for (size_t p = 0; p < matrix->indices_0.size; ++p) {
      size_t begin = 0, end = 0, outer = 0;
      bsp_array_read(matrix->indices_0, p, outer);
      bsp_array_read(matrix->pointers_to_1, p, begin);
      bsp_array_read(matrix->pointers_to_1, p + 1, end);
      for (size_t i = begin; i < end; ++i) {
        size_t coord[2] = {outer, 0};
        bsp_array_read(matrix->indices_1, i, coord[1]);
        push_entry(&result, coord, matrix->is_iso ? 0 : i);
      }
    }
    return result;
  }
  if (matrix->format == BSP_DVEC || matrix->format == BSP_DMAT ||
      matrix->format == BSP_DMATC) {
    size_t rows = matrix->format == BSP_DMATC ? matrix->ncols : matrix->nrows;
    size_t cols = result.rank == 1 ? 1
                                   : (matrix->format == BSP_DMATC
                                          ? matrix->nrows
                                          : matrix->ncols);
    for (size_t i = 0; i < matrix->nnz; ++i) {
      size_t coord[2] = {i / cols, i % cols};
      if (result.rank == 1) coord[0] = i;
      if (coord[0] >= rows) die("dense value count exceeds shape");
      push_entry(&result, coord, matrix->is_iso ? 0 : i);
    }
    return result;
  }
  die("unsupported predefined input format");
  return result;
}

/* A predefined format is only a named view of a custom level tree. */
static bsp_error_t write_predefined(const char *path, bsp_tensor_t tensor,
                                    const char *format, int compression) {
  bsp_matrix_t matrix;
  bsp_level_t *level = tensor.level;
  bsp_construct_default_matrix_t(&matrix);
  matrix.format = bsp_get_matrix_format((char *) format);
  matrix.nrows = tensor.dims[0];
  matrix.ncols = tensor.rank == 1 ? 1 : tensor.dims[1];
  matrix.nnz = tensor.nnz;
  matrix.is_iso = tensor.is_iso;
  matrix.structure = tensor.structure;
  matrix.values = bsp_get_tensor_values(tensor);

  if (matrix.format == BSP_INVALID_FORMAT) return BSP_ERROR_UNSUPPORTED;
  if (matrix.format == BSP_DVEC || matrix.format == BSP_DMAT ||
      matrix.format == BSP_DMATC) {
    return bsp_write_matrix(path, matrix, NULL, NULL, compression);
  }
  if (matrix.format == BSP_CVEC || matrix.format == BSP_COOR ||
      matrix.format == BSP_COOC) {
    bsp_sparse_t *sparse;
    if (level->kind != BSP_TENSOR_SPARSE) return BSP_ERROR_INTERNAL;
    sparse = level->data;
    matrix.indices_0 = sparse->indices[0];
    if (sparse->rank == 2) matrix.indices_1 = sparse->indices[1];
    return bsp_write_matrix(path, matrix, NULL, NULL, compression);
  }
  if (matrix.format == BSP_CSR || matrix.format == BSP_CSC) {
    bsp_dense_t *dense;
    bsp_sparse_t *sparse;
    if (level->kind != BSP_TENSOR_DENSE) return BSP_ERROR_INTERNAL;
    dense = level->data;
    if (dense->child->kind != BSP_TENSOR_SPARSE) return BSP_ERROR_INTERNAL;
    sparse = dense->child->data;
    matrix.pointers_to_1 = *sparse->pointers_to;
    matrix.indices_1 = sparse->indices[0];
    return bsp_write_matrix(path, matrix, NULL, NULL, compression);
  }
  if (matrix.format == BSP_DCSR || matrix.format == BSP_DCSC) {
    bsp_sparse_t *outer, *inner;
    if (level->kind != BSP_TENSOR_SPARSE) return BSP_ERROR_INTERNAL;
    outer = level->data;
    if (outer->child->kind != BSP_TENSOR_SPARSE) return BSP_ERROR_INTERNAL;
    inner = outer->child->data;
    matrix.indices_0 = outer->indices[0];
    matrix.pointers_to_1 = *inner->pointers_to;
    matrix.indices_1 = inner->indices[0];
    return bsp_write_matrix(path, matrix, NULL, NULL, compression);
  }
  return BSP_ERROR_UNSUPPORTED;
}

static int compare_rank;
static int compare_entry(const void *a_, const void *b_) {
  const entry_t *a = a_, *b = b_;
  for (int d = 0; d < compare_rank; ++d) {
    if (a->coord[d] < b->coord[d]) return -1;
    if (a->coord[d] > b->coord[d]) return 1;
  }
  return 0;
}

static void apply_transpose(entries_t *entries, const size_t *source,
                            const size_t *target) {
  size_t *copy = malloc((size_t) entries->rank * sizeof(*copy));
  if (copy == NULL) die("out of memory");
  for (size_t k = 0; k < entries->size; ++k) {
    memcpy(copy, entries->entry[k].coord,
           (size_t) entries->rank * sizeof(*copy));
    for (int td = 0; td < entries->rank; ++td) {
      int sd = 0;
      while (sd < entries->rank && source[sd] != target[td]) ++sd;
      if (sd == entries->rank) die("transpose is not a permutation");
      entries->entry[k].coord[td] = copy[sd];
    }
  }
  free(copy);
  compare_rank = entries->rank;
  qsort(entries->entry, entries->size, sizeof(*entries->entry), compare_entry);
}

static bsp_array_t copy_values(const bsp_array_t source, const entries_t *entries,
                               bool iso) {
  bsp_array_t values;
  size_t size = iso && entries->size != 0 ? 1 : entries->size;
  if (bsp_construct_array_t(&values, size, source.type) != BSP_SUCCESS)
    die("out of memory");
  for (size_t i = 0; i < size; ++i)
    memcpy((char *) values.data + i * bsp_type_size(values.type),
           (char *) source.data + entries->entry[iso ? 0 : i].value *
                                      bsp_type_size(source.type),
           bsp_type_size(source.type));
  return values;
}

static size_t prefix_equal(const entry_t *a, const entry_t *b, size_t depth,
                           int rank) {
  for (int d = 0; d < rank; ++d)
    if (a->coord[depth + (size_t) d] != b->coord[depth + (size_t) d]) return 0;
  return 1;
}

/* Recursively build a level.  parent_ptr partitions entries into incoming
 * fibers.  The entries are already lexicographically sorted. */
static bsp_level_t *build_level(cJSON *description, const entries_t *entries,
                                const size_t *dims, size_t depth,
                                const size_t *parent_ptr, size_t parents,
                                bool root, bsp_array_t values) {
  cJSON *desc = cJSON_GetObjectItemCaseSensitive(description, "level_desc");
  bsp_level_t *level = calloc(1, sizeof(*level));
  const char *name = cJSON_GetStringValue(desc);
  if (level == NULL) die("out of memory");
  if (name != NULL && strcmp(name, "element") == 0) {
    bsp_element_t *element = malloc(sizeof(*element));
    for (size_t p = 0; p < parents; ++p)
      if (parent_ptr[p + 1] - parent_ptr[p] != 1)
        die("each element position must have one value");
    if (element == NULL) die("out of memory");
    element->values = values;
    level->kind = BSP_TENSOR_ELEMENT;
    level->data = element;
    return level;
  }
  cJSON *rank_json = cJSON_GetObjectItemCaseSensitive(description, "rank");
  int rank = (int) cJSON_GetNumberValue(rank_json);
  cJSON *child_desc = cJSON_GetObjectItemCaseSensitive(description, "level");
  if (rank <= 0 || depth + (size_t) rank > (size_t) entries->rank ||
      !cJSON_IsObject(child_desc))
    die("invalid target level descriptor");

  if (strcmp(name ? name : "", "sparse") == 0) {
    bsp_sparse_t *sparse = calloc(1, sizeof(*sparse));
    size_t groups = 0, *child_ptr;
    if (sparse == NULL) die("out of memory");
    for (size_t p = 0; p < parents; ++p)
      for (size_t i = parent_ptr[p]; i < parent_ptr[p + 1]; ++i)
        if (i == parent_ptr[p] ||
            !prefix_equal(&entries->entry[i - 1], &entries->entry[i], depth,
                          rank))
          ++groups;
    sparse->rank = rank;
    sparse->indices = calloc((size_t) rank, sizeof(*sparse->indices));
    child_ptr = malloc((groups + 1) * sizeof(*child_ptr));
    if (sparse->indices == NULL || child_ptr == NULL) die("out of memory");
    for (int d = 0; d < rank; ++d)
      if (bsp_construct_array_t(&sparse->indices[d], groups,
                                bsp_pick_integer_type(dims[depth + (size_t) d])) !=
          BSP_SUCCESS)
        die("out of memory");
    if (!root) {
      sparse->pointers_to = malloc(sizeof(*sparse->pointers_to));
      if (sparse->pointers_to == NULL ||
          bsp_construct_array_t(sparse->pointers_to, parents + 1,
                                bsp_pick_integer_type(groups)) != BSP_SUCCESS)
        die("out of memory");
    }
    groups = 0;
    child_ptr[0] = 0;
    for (size_t p = 0; p < parents; ++p) {
      if (!root) {
        bsp_array_t pointers = *sparse->pointers_to;
        bsp_array_write(pointers, p, groups);
      }
      for (size_t i = parent_ptr[p]; i < parent_ptr[p + 1]; ++i) {
        if (i == parent_ptr[p] ||
            !prefix_equal(&entries->entry[i - 1], &entries->entry[i], depth,
                          rank)) {
          if (groups != 0) child_ptr[groups] = i;
          for (int d = 0; d < rank; ++d)
            bsp_array_write(sparse->indices[d], groups,
                            entries->entry[i].coord[depth + (size_t) d]);
          ++groups;
        }
      }
    }
    child_ptr[groups] = entries->size;
    if (!root) {
      bsp_array_t pointers = *sparse->pointers_to;
      bsp_array_write(pointers, parents, groups);
    }
    sparse->child = build_level(child_desc, entries, dims, depth + (size_t) rank,
                                child_ptr, groups, false, values);
    free(child_ptr);
    level->kind = BSP_TENSOR_SPARSE;
    level->data = sparse;
    return level;
  }

  if (strcmp(name ? name : "", "dense") == 0) {
    bsp_dense_t *dense = malloc(sizeof(*dense));
    size_t width = 1, children, *child_ptr;
    if (dense == NULL) die("out of memory");
    for (int d = 0; d < rank; ++d) width *= dims[depth + (size_t) d];
    children = parents * width;
    child_ptr = malloc((children + 1) * sizeof(*child_ptr));
    if (child_ptr == NULL) die("out of memory");
    size_t at = 0;
    for (size_t p = 0; p < parents; ++p) {
      size_t end = parent_ptr[p + 1];
      for (size_t suffix = 0; suffix < width; ++suffix) {
        child_ptr[p * width + suffix] = at;
        while (at < end) {
          size_t n = suffix;
          int equal = 1;
          for (int d = rank - 1; d >= 0; --d) {
            size_t dim = dims[depth + (size_t) d];
            if (entries->entry[at].coord[depth + (size_t) d] != n % dim)
              equal = 0;
            n /= dim;
          }
          if (!equal) break;
          ++at;
        }
      }
    }
    child_ptr[children] = entries->size;
    dense->rank = rank;
    dense->child = build_level(child_desc, entries, dims,
                               depth + (size_t) rank, child_ptr, children,
                               false, values);
    free(child_ptr);
    level->kind = BSP_TENSOR_DENSE;
    level->data = dense;
    return level;
  }
  die("unknown target level descriptor");
  return NULL;
}

static cJSON *predefined_level(const char *format, size_t *transpose, int rank) {
  const char *json = NULL;
  if (!strcmp(format, "DVEC")) json = "{\"level_desc\":\"dense\",\"rank\":1,\"level\":{\"level_desc\":\"element\"}}";
  else if (!strcmp(format, "CVEC")) json = "{\"level_desc\":\"sparse\",\"rank\":1,\"level\":{\"level_desc\":\"element\"}}";
  else if (!strcmp(format, "DMAT") || !strcmp(format, "DMATR")) json = "{\"level_desc\":\"dense\",\"rank\":2,\"level\":{\"level_desc\":\"element\"}}";
  else if (!strcmp(format, "DMATC")) { json = "{\"level_desc\":\"dense\",\"rank\":2,\"level\":{\"level_desc\":\"element\"}}"; transpose[0]=1; transpose[1]=0; }
  else if (!strcmp(format, "COO") || !strcmp(format, "COOR")) json = "{\"level_desc\":\"sparse\",\"rank\":2,\"level\":{\"level_desc\":\"element\"}}";
  else if (!strcmp(format, "COOC")) { json = "{\"level_desc\":\"sparse\",\"rank\":2,\"level\":{\"level_desc\":\"element\"}}"; transpose[0]=1; transpose[1]=0; }
  else if (!strcmp(format, "CSR")) json = "{\"level_desc\":\"dense\",\"rank\":1,\"level\":{\"level_desc\":\"sparse\",\"rank\":1,\"level\":{\"level_desc\":\"element\"}}}";
  else if (!strcmp(format, "CSC")) { json = "{\"level_desc\":\"dense\",\"rank\":1,\"level\":{\"level_desc\":\"sparse\",\"rank\":1,\"level\":{\"level_desc\":\"element\"}}}"; transpose[0]=1; transpose[1]=0; }
  else if (!strcmp(format, "DCSR")) json = "{\"level_desc\":\"sparse\",\"rank\":1,\"level\":{\"level_desc\":\"sparse\",\"rank\":1,\"level\":{\"level_desc\":\"element\"}}}";
  else if (!strcmp(format, "DCSC")) { json = "{\"level_desc\":\"sparse\",\"rank\":1,\"level\":{\"level_desc\":\"sparse\",\"rank\":1,\"level\":{\"level_desc\":\"element\"}}}"; transpose[0]=1; transpose[1]=0; }
  if (json == NULL || (rank != 1 && rank != 2)) return NULL;
  return cJSON_Parse(json);
}

static void usage(void) {
  fprintf(stderr, "usage: reformat INPUT OUTPUT TARGET_HEADER.json [COMPRESSION]\n");
  exit(2);
}

int main(int argc, char **argv) {
  cJSON *source_header, *target_header, *format_json, *custom, *description;
  const char *source_format, *target_format;
  bsp_tensor_t tensor = bsp_construct_default_tensor_t();
  bsp_matrix_t matrix;
  bsp_array_t source_values, values;
  entries_t entries;
  size_t *source_transpose, *target_transpose, *stored_dims, root_ptr[2];
  int rank, compression = 0;
  bool source_is_tensor, iso;
  if (argc < 4 || argc > 5) usage();
  if (argc == 5) compression = atoi(argv[4]);
  if (compression < 0 || compression > 9) die("compression must be 0 through 9");

  source_header = input_header(argv[1]);
  target_header = read_json(argv[3]);
  format_json = cJSON_GetObjectItemCaseSensitive(source_header, "format");
  source_format = cJSON_GetStringValue(format_json);
  target_format = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(target_header, "format"));
  if (source_format == NULL || target_format == NULL) die("header has no format string");
  source_is_tensor = strcmp(source_format, "custom") == 0;

  if (source_is_tensor) {
    tensor = bsp_read_tensor(argv[1], NULL);
    if (tensor.level == NULL) die("failed to parse input tensor");
    tensor.is_iso = header_values_are_iso(source_header);
    rank = tensor.rank;
    source_values = bsp_get_tensor_values(tensor);
    iso = tensor.is_iso;
    entries = tensor_to_coo(&tensor);
  } else {
    if (bsp_read_matrix(&matrix, argv[1], NULL) != BSP_SUCCESS)
      die("failed to parse input matrix");
    rank = bsp_matrix_format_is_vector(matrix.format) ? 1 : 2;
    source_values = matrix.values;
    iso = matrix.is_iso;
    entries = matrix_to_coo(&matrix);
  }

  source_transpose = malloc((size_t) rank * sizeof(*source_transpose));
  target_transpose = malloc((size_t) rank * sizeof(*target_transpose));
  stored_dims = malloc((size_t) rank * sizeof(*stored_dims));
  if (!source_transpose || !target_transpose || !stored_dims) die("out of memory");
  for (int d = 0; d < rank; ++d) {
    source_transpose[d] = source_is_tensor && tensor.transpose ? tensor.transpose[d] : (size_t) d;
    target_transpose[d] = (size_t) d;
  }
  if (!source_is_tensor && rank == 2 &&
      (matrix.format == BSP_DMATC || matrix.format == BSP_CSC ||
       matrix.format == BSP_DCSC || matrix.format == BSP_COOC)) {
    source_transpose[0] = 1;
    source_transpose[1] = 0;
  }

  if (!strcmp(target_format, "custom")) {
    custom = cJSON_GetObjectItemCaseSensitive(target_header, "custom");
    description = cJSON_GetObjectItemCaseSensitive(custom, "level");
    cJSON *transpose = cJSON_GetObjectItemCaseSensitive(custom, "transpose");
    if (!cJSON_IsObject(description)) die("custom target requires custom.level");
    if (transpose != NULL) {
      if (cJSON_GetArraySize(transpose) != rank) die("transpose rank mismatch");
      for (int d = 0; d < rank; ++d)
        target_transpose[d] = (size_t) cJSON_GetNumberValue(cJSON_GetArrayItem(transpose, d));
    }
    description = cJSON_Duplicate(description, 1);
  } else {
    description = predefined_level(target_format, target_transpose, rank);
    if (description == NULL) die("unknown or rank-incompatible target format");
  }

  apply_transpose(&entries, source_transpose, target_transpose);
  for (int d = 0; d < rank; ++d) {
    size_t logical = target_transpose[d];
    stored_dims[d] = source_is_tensor ? tensor.dims[logical]
                                    : (logical == 0 ? matrix.nrows : matrix.ncols);
  }
  values = copy_values(source_values, &entries, iso);
  bsp_tensor_t result = bsp_construct_default_tensor_t();
  result.rank = rank;
  result.nnz = entries.size;
  result.is_iso = iso;
  result.structure = source_is_tensor ? tensor.structure : matrix.structure;
  result.dims = malloc((size_t) rank * sizeof(*result.dims));
  result.transpose = malloc((size_t) rank * sizeof(*result.transpose));
  if (!result.dims || !result.transpose) die("out of memory");
  for (int d = 0; d < rank; ++d) {
    result.dims[d] = source_is_tensor ? tensor.dims[d] : (d == 0 ? matrix.nrows : matrix.ncols);
    result.transpose[d] = target_transpose[d];
  }
  root_ptr[0] = 0; root_ptr[1] = entries.size;
  result.level = build_level(description, &entries, stored_dims, 0, root_ptr, 1, true, values);
  if (!strcmp(target_format, "custom")) {
    cJSON *user = cJSON_CreateObject();
    if (bsp_write_tensor(argv[2], result, NULL, user, compression) != BSP_SUCCESS)
      die("failed to write output tensor");
    cJSON_Delete(user);
  } else if (write_predefined(argv[2], result, target_format, compression) !=
             BSP_SUCCESS) {
    die("failed to write predefined output");
  }

  bsp_destroy_tensor_t(result);
  free_entries(&entries);
  if (source_is_tensor) bsp_destroy_tensor_t(tensor); else bsp_destroy_matrix_t(&matrix);
  free(source_transpose); free(target_transpose); free(stored_dims);
  cJSON_Delete(description); cJSON_Delete(source_header); cJSON_Delete(target_header);
  return 0;
}
