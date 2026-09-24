# SPDX-FileCopyrightText: 2024 Binsparse Developers
#
# SPDX-License-Identifier: BSD-3-Clause

target_sources(binsparse PRIVATE
  read_matrix.c
  read_tensor.c
  version.c
  write_matrix.c
  write_tensor.c
)

target_sources(binsparse_dynamic PRIVATE
  read_matrix.c
  read_tensor.c
  version.c
  write_matrix.c
  write_tensor.c
)
