# SPDX-FileCopyrightText: 2024 Binsparse Developers
#
# SPDX-License-Identifier: BSD-3-Clause

function(download_data url file_name)
  set(DATASET_ARCHIVE ${CMAKE_BINARY_DIR}/data/${file_name})

  file(DOWNLOAD
       ${url}
       ${DATASET_ARCHIVE})
endfunction()

download_data(https://suitesparse-collection-website.herokuapp.com/MM/DIMACS10/chesapeake.tar.gz
              chesapeake.tar.gz)

download_data(https://suitesparse-collection-website.herokuapp.com/MM/HB/1138_bus.tar.gz
              1138_bus.tar.gz)

download_data(https://suitesparse-collection-website.herokuapp.com/MM/Belcastro/mouse_gene.tar.gz
              mouse_gene.tar.gz)

download_data(https://suitesparse-collection-website.herokuapp.com/MM/Pajek/IMDB.tar.gz
              IMDB.tar.gz)

download_data(https://suitesparse-collection-website.herokuapp.com/MM/Bai/dwg961a.tar.gz
              dwg961a.tar.gz)

find_program(BASH_PROGRAM bash)

enable_testing()

if(BASH_PROGRAM)
     add_test(NAME integration.chesapeake COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CMAKE_BINARY_DIR}/data/chesapeake.tar.gz)
     add_test(NAME integration.1138_bus COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CMAKE_BINARY_DIR}/data/1138_bus.tar.gz)
     add_test(NAME integration.mouse_gene COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CMAKE_BINARY_DIR}/data/mouse_gene.tar.gz)
     add_test(NAME integration.IMDB COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CMAKE_BINARY_DIR}/data/IMDB.tar.gz)
     add_test(NAME integration.dwg961a COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CMAKE_BINARY_DIR}/data/dwg961a.tar.gz)

     add_test(NAME integration.cpp.chesapeake COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test-cpp.sh ${CMAKE_BINARY_DIR}/data/chesapeake.tar.gz)
     add_test(NAME integration.cpp.1138_bus COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test-cpp.sh ${CMAKE_BINARY_DIR}/data/1138_bus.tar.gz)
     add_test(NAME integration.cpp.mouse_gene COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test-cpp.sh ${CMAKE_BINARY_DIR}/data/mouse_gene.tar.gz)
     add_test(NAME integration.cpp.IMDB COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test-cpp.sh ${CMAKE_BINARY_DIR}/data/IMDB.tar.gz)
     add_test(NAME integration.cpp.dwg961a COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test-cpp.sh ${CMAKE_BINARY_DIR}/data/dwg961a.tar.gz)
endif()
