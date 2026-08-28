/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "reformat.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
  cJSON* header;
  bsp_error_t error;

  if (argc != 3) {
    fprintf(stderr, "usage: binsparse_to_binsparse <tensor_in> <tensor_out>\n");
    return 2;
  }
  header = input_header(argv[1]);
  cJSON* format = cJSON_GetObjectItemCaseSensitive(header, "format");
  const char* name = cJSON_GetStringValue(format);
  if (name != NULL && strcmp(name, "DMAT") == 0)
    cJSON_SetValuestring(format, "DMATR");
  else if (name != NULL && strcmp(name, "COO") == 0)
    cJSON_SetValuestring(format, "COOR");
  error = bsp_reformat_file(argv[1], argv[2], header, 0);
  cJSON_Delete(header);
  if (error != BSP_SUCCESS) {
    fprintf(stderr, "binsparse_to_binsparse: %s\n",
            bsp_get_error_string(error));
    return 1;
  }
  return 0;
}
