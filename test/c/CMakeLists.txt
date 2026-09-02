# SPDX-FileCopyrightText: 2026 Binsparse Developers
#
# SPDX-License-Identifier: BSD-3-Clause

add_executable(dvec_shape_test dvec_shape_test.c)
target_link_libraries(dvec_shape_test binsparse)

add_executable(version_test version_test.c)
target_link_libraries(version_test binsparse)

add_test(
  NAME unit.dvec_shape
  COMMAND dvec_shape_test ${CMAKE_CURRENT_BINARY_DIR}/dvec_shape_test.h5)

add_test(
  NAME unit.version
  COMMAND version_test ${CMAKE_CURRENT_BINARY_DIR}/version_test.h5)
