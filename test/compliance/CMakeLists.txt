find_package(ZLIB REQUIRED)

add_library(cnpy_vendor STATIC
  ${PROJECT_SOURCE_DIR}/third_party/cnpy/cnpy.cpp
  cnpy_c.cpp
)
target_include_directories(cnpy_vendor PUBLIC
  ${PROJECT_SOURCE_DIR}/third_party/cnpy
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(cnpy_vendor PUBLIC ZLIB::ZLIB)
target_compile_features(cnpy_vendor PUBLIC cxx_std_11)

add_library(compliance_core STATIC reformat.c)
target_include_directories(compliance_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(compliance_core PUBLIC binsparse)

add_executable(npy_to_binsparse npy_to_binsparse.c)
target_link_libraries(npy_to_binsparse PRIVATE compliance_core cnpy_vendor)
set_target_properties(npy_to_binsparse PROPERTIES LINKER_LANGUAGE CXX)

add_executable(binsparse_to_npy binsparse_to_npy.c)
target_link_libraries(binsparse_to_npy PRIVATE compliance_core cnpy_vendor)
set_target_properties(binsparse_to_npy PROPERTIES LINKER_LANGUAGE CXX)

add_executable(binsparse_to_binsparse binsparse_to_binsparse.c)
target_link_libraries(binsparse_to_binsparse PRIVATE compliance_core)
