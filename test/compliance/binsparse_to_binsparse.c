#include "reformat.h"

#include <stdio.h>

int main(int argc, char **argv) {
  cJSON *header;
  bsp_error_t error;

  if (argc != 3) {
    fprintf(stderr, "usage: binsparse_to_binsparse <tensor_in> <tensor_out>\n");
    return 2;
  }
  header = input_header(argv[1]);
  error = bsp_reformat_file(argv[1], argv[2], header, 0);
  cJSON_Delete(header);
  if (error != BSP_SUCCESS) {
    fprintf(stderr, "binsparse_to_binsparse: %s\n",
            bsp_get_error_string(error));
    return 1;
  }
  return 0;
}
