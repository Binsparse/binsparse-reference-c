#[[
SPDX-FileCopyrightText: 2026 Binsparse Developers

SPDX-License-Identifier: BSD-3-Clause
]]

add_library(npy_bridge STATIC
  npy_c.c
)
target_include_directories(npy_bridge PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(compliance_core STATIC reformat.c)
target_include_directories(compliance_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(compliance_core PUBLIC binsparse)

add_executable(npy_to_binsparse npy_to_binsparse.c)
target_link_libraries(npy_to_binsparse PRIVATE compliance_core npy_bridge)

add_executable(binsparse_to_npy binsparse_to_npy.c)
target_link_libraries(binsparse_to_npy PRIVATE compliance_core npy_bridge)

add_executable(binsparse_to_binsparse binsparse_to_binsparse.c)
target_link_libraries(binsparse_to_binsparse PRIVATE compliance_core)
