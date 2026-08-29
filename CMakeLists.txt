# SPDX-FileCopyrightText: 2024 Binsparse Developers
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.20)
project(binsparse VERSION 1.0.0)

cmake_policy(SET CMP0079 NEW)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# FIXME: -march=native is not portable
# set(CMAKE_C_FLAGS "-O3 -march=native")
set(CMAKE_C_FLAGS "-g ")
set(CMAKE_CXX_FLAGS "-g ")

option(ENABLE_SANITIZERS "Enable Clang sanitizers" OFF)

include(GNUInstallDirs)

add_library(binsparse STATIC)
add_library(binsparse_dynamic SHARED)

add_subdirectory(include)
add_subdirectory(src)

# NOTE: For now, both HDF5 and cJSON are `PUBLIC`, meaning that anything that
# depends on `binsparse` will also link/include HDF5 and cJSON. We can change
# these to `PRIVATE` to use them only when building binsparse.

find_package(HDF5 REQUIRED COMPONENTS C)
target_link_libraries(binsparse PUBLIC ${HDF5_C_LIBRARIES})
target_link_libraries(binsparse_dynamic PUBLIC ${HDF5_C_LIBRARIES})

include(FetchContent)

# Force cJSON to build as static library for MEX compatibility
set(BUILD_SHARED_LIBS_BACKUP ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Build cJSON as static library")
set(ENABLE_CJSON_TEST OFF CACHE INTERNAL "Disable cJSON tests")

FetchContent_Declare(
  cJSON
  # GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
  GIT_REPOSITORY https://github.com/p1k0chu/cJSON.git
  GIT_TAG 887642c0a93bd8a6616bf90daacac0ea7d4b095e
)
FetchContent_MakeAvailable(cJSON)

# cpp-semver is header-only. Use strict SemVer parsing for file-format metadata.
set(SEMVER_BUILD_TESTS OFF CACHE INTERNAL "Disable cpp-semver tests")
set(SEMVER_INSTALL OFF CACHE INTERNAL "Disable cpp-semver install")
FetchContent_Declare(
  cpp-semver
  GIT_REPOSITORY https://github.com/z4kn4fein/cpp-semver.git
  GIT_TAG v0.4.0
)
FetchContent_MakeAvailable(cpp-semver)

# Restore original BUILD_SHARED_LIBS setting
set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_BACKUP})

configure_file(${cJSON_SOURCE_DIR}/cJSON.h ${CMAKE_BINARY_DIR}/include/cJSON/cJSON.h)
target_link_libraries(binsparse PRIVATE cjson)
target_link_libraries(binsparse_dynamic PRIVATE cjson)
target_link_libraries(binsparse PRIVATE $<BUILD_INTERFACE:semver>)
target_link_libraries(binsparse_dynamic PRIVATE $<BUILD_INTERFACE:semver>)

# Set up include directories properly for both build and install
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        ${HDF5_INCLUDE_DIRS})

target_include_directories(binsparse_dynamic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        ${HDF5_INCLUDE_DIRS})

# Installation rules - these are always needed when the library is built
install(TARGETS binsparse
        EXPORT binsparse-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS binsparse_dynamic
        EXPORT binsparse-targets_dynamic
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/binsparse
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
                       PATTERN "*.hpp")

# Export targets
install(EXPORT binsparse-targets
        FILE binsparse-targets.cmake
        NAMESPACE binsparse::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/binsparse)
install(EXPORT binsparse-targets_dynamic
        FILE binsparse-targets_dynamic.cmake
        NAMESPACE binsparse::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/binsparse)

# Create and install package config files
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/binsparse-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/binsparse-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/binsparse-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/binsparse)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/binsparse-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/binsparse-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/binsparse)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  enable_testing()

  add_subdirectory(test/compliance)

  if (ENABLE_SANITIZERS)
      set(SANITIZER_FLAGS "-fsanitize=address,undefined")
      target_compile_options(binsparse INTERFACE ${SANITIZER_FLAGS} -g -O1 -fno-omit-frame-pointer)
      target_link_options(binsparse INTERFACE ${SANITIZER_FLAGS})
      target_compile_options(binsparse_dynamic INTERFACE ${SANITIZER_FLAGS} -g -O1 -fno-omit-frame-pointer)
      target_link_options(binsparse_dynamic INTERFACE ${SANITIZER_FLAGS})
  endif()

  add_subdirectory(examples)
  add_subdirectory(test)
endif()
