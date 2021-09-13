cmake_minimum_required(VERSION 3.5)

project(fstalign LANGUAGES CXX C)

include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

enable_testing()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED ENV{OPENFST_ROOT})
  set(OPENFST_ROOT $ENV{OPENFST_ROOT} CACHE STRING "Path to OpenFST")
endif()
message(STATUS "OpenFST root: ${OPENFST_ROOT}")

set(FSTALIGN_INCLUDES
  ${PROJECT_SOURCE_DIR}/third-party/spdlog/include
  ${PROJECT_SOURCE_DIR}/third-party/CLI11/include
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/third-party
  ${PROJECT_SOURCE_DIR}/third-party/inih
  ${PROJECT_SOURCE_DIR}/third-party/inih/cpp
)

find_package(Threads REQUIRED)

set(FSTALIGN_LIBRARIES
  jsoncpp_lib_static
  ${PROJECT_SOURCE_DIR}/third-party/inih/ini.c
)

set(OPENFST_INCLUDES
  ${OPENFST_ROOT}/include
)

if(DYNAMIC_OPENFST)
  set(OPENFST_LIBRARIES
    ${OPENFST_ROOT}/lib/libfst.so
  )
else()
  set(OPENFST_LIBRARIES
    ${OPENFST_ROOT}/lib/libfst.a -ldl
  )
endif()

add_library(fstaligner-common
  src/fstalign.cpp
  src/wer.cpp
  src/fast-d.cpp
  src/AdaptedComposition.cpp
  src/StandardComposition.cpp
  src/AlignmentTraversor.cpp
  src/Ctm.cpp
  src/FstLoader.cpp
  src/FstFileLoader.cpp
  src/logging.cpp
  src/Nlp.cpp
  src/OneBestFstLoader.cpp
  src/PathHeap.cpp
  src/SynonymEngine.cpp
  src/utilities.cpp
  src/Walker.cpp
  third-party/inih/cpp/INIReader.cpp
)

target_link_libraries(fstaligner-common
  Threads::Threads
  ${FSTALIGN_LIBRARIES}
  ${FST_KALDI_LIBRARIES}
)

add_subdirectory(third-party/jsoncpp)
add_subdirectory(third-party/catch2)

add_executable(fstalign src/main.cpp)

include_directories(fstalign 
  ${FSTALIGN_INCLUDES}
  ${OPENFST_INCLUDES}
)

target_link_libraries(fstalign
  fstaligner-common
  ${CMAKE_DL_LIBS}
  ${FSTALIGN_LIBRARIES}
  ${OPENFST_LIBRARIES}
)

add_subdirectory(test)
