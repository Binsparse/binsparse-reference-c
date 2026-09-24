/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "npy_c.h"

#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  size_t word_size;
  bool fortran_order;
  size_t rank;
  size_t* shape;
} npy_header_t;

static const char* skip_spaces(const char* text) {
  while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r')
    ++text;
  return text;
}

static const char* value_for_key(const char* header, const char* key) {
  const char* key_start = strstr(header, key);
  if (key_start == NULL)
    return NULL;
  const char* value = skip_spaces(key_start + strlen(key));
  if (*value != ':')
    return NULL;
  return skip_spaces(value + 1);
}

static int parse_descr(const char* header, char* kind, size_t* word_size) {
  const char* value = value_for_key(header, "'descr'");
  if (value == NULL)
    value = value_for_key(header, "\"descr\"");
  if (value == NULL || (*value != '\'' && *value != '"'))
    return 1;

  char quote = *value++;
  char endian = *value++;
  if (endian != '<' && endian != '>' && endian != '|' && endian != '=')
    return 1;
  *kind = *value++;
  if (*kind != 'b' && *kind != 'i' && *kind != 'u' && *kind != 'f' &&
      *kind != 'c')
    return 1;

  char* end = NULL;
  unsigned long parsed = strtoul(value, &end, 10);
  if (end == value || *end != quote || parsed == 0)
    return 1;
  *word_size = (size_t) parsed;
  return 0;
}

static int parse_fortran_order(const char* header, bool* fortran_order) {
  const char* value = value_for_key(header, "'fortran_order'");
  if (value == NULL)
    value = value_for_key(header, "\"fortran_order\"");
  if (value == NULL)
    return 1;
  if (!strncmp(value, "True", 4)) {
    *fortran_order = true;
    return 0;
  }
  if (!strncmp(value, "False", 5)) {
    *fortran_order = false;
    return 0;
  }
  return 1;
}

static int append_shape_dim(size_t** shape, size_t* rank, size_t dim) {
  size_t* resized = realloc(*shape, (*rank + 1) * sizeof(size_t));
  if (resized == NULL)
    return 1;
  resized[*rank] = dim;
  *shape = resized;
  ++*rank;
  return 0;
}

static int parse_shape(const char* header, size_t** shape, size_t* rank) {
  const char* value = value_for_key(header, "'shape'");
  if (value == NULL)
    value = value_for_key(header, "\"shape\"");
  if (value == NULL)
    return 1;
  value = skip_spaces(value);
  if (*value != '(')
    return 1;
  ++value;

  *shape = NULL;
  *rank = 0;
  for (;;) {
    value = skip_spaces(value);
    if (*value == ')')
      return 0;

    char* end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end == value)
      return 1;
    if (append_shape_dim(shape, rank, (size_t) parsed))
      return 1;

    value = skip_spaces(end);
    if (*value == ',') {
      ++value;
      continue;
    }
    if (*value == ')')
      return 0;
    return 1;
  }
}

static void destroy_header(npy_header_t* header) {
  free(header->shape);
  memset(header, 0, sizeof(*header));
}

static uint16_t read_le16(const uint8_t bytes[2]) {
  return (uint16_t) bytes[0] | ((uint16_t) bytes[1] << 8);
}

static uint32_t read_le32(const uint8_t bytes[4]) {
  return (uint32_t) bytes[0] | ((uint32_t) bytes[1] << 8) |
         ((uint32_t) bytes[2] << 16) | ((uint32_t) bytes[3] << 24);
}

static int read_header(FILE* file, npy_header_t* out) {
  uint8_t prefix[10];
  if (fread(prefix, 1, sizeof(prefix), file) != sizeof(prefix))
    return 1;
  const uint8_t magic[] = {0x93, 'N', 'U', 'M', 'P', 'Y'};
  if (memcmp(prefix, magic, sizeof(magic)))
    return 1;

  uint8_t major = prefix[6];
  uint8_t minor = prefix[7];
  size_t header_size;
  if (major == 1 && minor == 0) {
    header_size = read_le16(prefix + 8);
  } else if ((major == 2 || major == 3) && minor == 0) {
    uint8_t length[4];
    length[0] = prefix[8];
    length[1] = prefix[9];
    if (fread(length + 2, 1, 2, file) != 2)
      return 1;
    header_size = read_le32(length);
  } else {
    return 1;
  }

  char* header = malloc(header_size + 1);
  if (header == NULL)
    return 1;
  if (fread(header, 1, header_size, file) != header_size) {
    free(header);
    return 1;
  }
  header[header_size] = '\0';

  char kind;
  int error = parse_descr(header, &kind, &out->word_size) ||
              parse_fortran_order(header, &out->fortran_order) ||
              parse_shape(header, &out->shape, &out->rank);
  free(header);
  return error;
}

static int checked_mul(size_t lhs, size_t rhs, size_t* result) {
  if (rhs != 0 && lhs > (size_t) -1 / rhs)
    return 1;
  *result = lhs * rhs;
  return 0;
}

static int element_count(const size_t* shape, size_t rank, size_t* count) {
  *count = 1;
  for (size_t i = 0; i < rank; ++i)
    if (checked_mul(*count, shape[i], count))
      return 1;
  return 0;
}

extern int npy_c_load(const char* path, npy_c_array* out) {
  FILE* file = fopen(path, "rb");
  if (file == NULL)
    return 1;

  npy_header_t header = {0};
  if (read_header(file, &header)) {
    fclose(file);
    destroy_header(&header);
    return 1;
  }

  size_t count;
  size_t bytes;
  if (element_count(header.shape, header.rank, &count) ||
      checked_mul(count, header.word_size, &bytes)) {
    fclose(file);
    destroy_header(&header);
    return 1;
  }

  out->size = count;
  out->word_size = header.word_size;
  out->rank = header.rank;
  out->shape = header.rank ? malloc(header.rank * sizeof(size_t)) : NULL;
  out->strides = header.rank ? malloc(header.rank * sizeof(size_t)) : NULL;
  out->data = bytes ? malloc(bytes) : NULL;
  if ((header.rank && (!out->shape || !out->strides)) ||
      (bytes && out->data == NULL)) {
    fclose(file);
    destroy_header(&header);
    npy_c_destroy(out);
    return 1;
  }
  if (header.rank)
    memcpy(out->shape, header.shape, header.rank * sizeof(size_t));

  size_t stride = 1;
  if (header.fortran_order) {
    for (size_t d = 0; d < out->rank; ++d) {
      out->strides[d] = stride;
      stride *= out->shape[d];
    }
  } else {
    for (size_t d = out->rank; d-- > 0;) {
      out->strides[d] = stride;
      stride *= out->shape[d];
    }
  }

  int error = bytes && fread(out->data, 1, bytes, file) != bytes;
  fclose(file);
  destroy_header(&header);
  return error;
}

extern void npy_c_destroy(npy_c_array* array) {
  free(array->data);
  free(array->shape);
  free(array->strides);
  memset(array, 0, sizeof(*array));
}

static char endian_char(void) {
  const uint16_t value = 1;
  return *(const uint8_t*) &value ? '<' : '>';
}

static int write_shape(char* buffer, size_t size, const size_t* shape,
                       size_t rank) {
  size_t used = 0;
  int written = snprintf(buffer, size, "(");
  if (written < 0 || (size_t) written >= size)
    return -1;
  used += (size_t) written;

  for (size_t i = 0; i < rank; ++i) {
    written = snprintf(buffer + used, size - used, "%s%zu", i == 0 ? "" : ", ",
                       shape[i]);
    if (written < 0 || (size_t) written >= size - used)
      return -1;
    used += (size_t) written;
  }
  if (rank == 1) {
    written = snprintf(buffer + used, size - used, ",");
    if (written < 0 || (size_t) written >= size - used)
      return -1;
    used += (size_t) written;
  }
  written = snprintf(buffer + used, size - used, ")");
  if (written < 0 || (size_t) written >= size - used)
    return -1;
  return 0;
}

static int write_npy(const char* path, const void* data, const size_t* shape,
                     size_t rank, size_t word_size, char kind) {
  FILE* file = fopen(path, "wb");
  if (file == NULL)
    return 1;

  char shape_text[512];
  if (write_shape(shape_text, sizeof(shape_text), shape, rank)) {
    fclose(file);
    return 1;
  }

  char header[1024];
  char endian = word_size == 1 ? '|' : endian_char();
  int header_length =
      snprintf(header, sizeof(header),
               "{'descr': '%c%c%zu', 'fortran_order': False, 'shape': %s, }",
               endian, kind, word_size, shape_text);
  if (header_length < 0 || (size_t) header_length >= sizeof(header)) {
    fclose(file);
    return 1;
  }

  size_t header_size = (size_t) header_length;
  size_t padding = 16 - (10 + header_size) % 16;
  if (header_size + padding >= sizeof(header)) {
    fclose(file);
    return 1;
  }
  memset(header + header_size, ' ', padding);
  header_size += padding;
  header[header_size - 1] = '\n';

  const uint8_t magic[] = {0x93, 'N', 'U', 'M', 'P', 'Y', 0x01, 0x00};
  uint8_t header_size_bytes[] = {
      (uint8_t) (header_size & 0xff),
      (uint8_t) ((header_size >> 8) & 0xff),
  };
  size_t count;
  if (element_count(shape, rank, &count)) {
    fclose(file);
    return 1;
  }

  int ok = fwrite(magic, 1, sizeof(magic), file) == sizeof(magic) &&
           fwrite(header_size_bytes, 1, sizeof(header_size_bytes), file) ==
               sizeof(header_size_bytes) &&
           fwrite(header, 1, header_size, file) == header_size &&
           (count == 0 || fwrite(data, word_size, count, file) == count);
  return fclose(file) == 0 && ok ? 0 : 1;
}

extern int npy_c_save(const char* path, const void* data, const size_t* shape,
                      size_t rank, int type) {
  switch (type) {
  case 0:
    return write_npy(path, data, shape, rank, sizeof(uint8_t), 'u');
  case 1:
    return write_npy(path, data, shape, rank, sizeof(uint16_t), 'u');
  case 2:
    return write_npy(path, data, shape, rank, sizeof(uint32_t), 'u');
  case 3:
    return write_npy(path, data, shape, rank, sizeof(uint64_t), 'u');
  case 4:
    return write_npy(path, data, shape, rank, sizeof(int8_t), 'i');
  case 5:
    return write_npy(path, data, shape, rank, sizeof(int16_t), 'i');
  case 6:
    return write_npy(path, data, shape, rank, sizeof(int32_t), 'i');
  case 7:
    return write_npy(path, data, shape, rank, sizeof(int64_t), 'i');
  case 8:
    return write_npy(path, data, shape, rank, sizeof(float), 'f');
  case 9:
    return write_npy(path, data, shape, rank, sizeof(double), 'f');
  case 10:
    return write_npy(path, data, shape, rank, sizeof(bool), 'b');
  case 11:
    return write_npy(path, data, shape, rank, sizeof(float _Complex), 'c');
  case 12:
    return write_npy(path, data, shape, rank, sizeof(double _Complex), 'c');
  default:
    return 1;
  }
}
