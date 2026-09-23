# SPDX-FileCopyrightText: 2024 Binsparse Developers
#
# SPDX-License-Identifier: BSD-3-Clause

find_program(JULIA_PROGRAM julia)

enable_testing()

if(JULIA_PROGRAM)
  add_test(NAME tensors.tensor_test COMMAND ${JULIA_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/tensor_test.jl)

  add_test(NAME tensors.cpp.tensor_test COMMAND ${JULIA_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/tensor_test.jl cpp)
endif()
