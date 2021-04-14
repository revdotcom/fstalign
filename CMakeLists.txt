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

if(DEFINED ENV{KALDI_ROOT})
  set(FST_KALDI_ROOT $ENV{KALDI_ROOT} CACHE STRING "Path to Kaldi")
endif()
message(STATUS "Kaldi root: ${FST_KALDI_ROOT}")

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

set(FST_KALDI_INCLUDES
  ${FST_KALDI_ROOT}/tools/OpenBLAS/install/include
  ${FST_KALDI_ROOT}/src
  ${FST_KALDI_ROOT}/tools/openfst/include
)

if(DYNAMIC_KALDI)
  set(FST_KALDI_LIBRARIES
    ${FST_KALDI_ROOT}/src/fstext/libkaldi-fstext.so
    ${FST_KALDI_ROOT}/src/util/libkaldi-util.so
    ${FST_KALDI_ROOT}/src/matrix/libkaldi-matrix.so
    ${FST_KALDI_ROOT}/src/base/libkaldi-base.so

    ${FST_KALDI_ROOT}/tools/openfst/lib/libfst.so
  )
else()
  set(FST_KALDI_LIBRARIES
    ${FST_KALDI_ROOT}/src/fstext/kaldi-fstext.a
    ${FST_KALDI_ROOT}/src/util/kaldi-util.a
    ${FST_KALDI_ROOT}/src/matrix/kaldi-matrix.a
    ${FST_KALDI_ROOT}/src/base/kaldi-base.a

    ${FST_KALDI_ROOT}/tools/openfst/lib/libfst.a

    ${FST_KALDI_ROOT}/tools/OpenBLAS/install/lib/libopenblas.a -lgfortran -ldl
  )
endif()

add_library(fstaligner-common
  src/fstalign.cpp
  src/wer.cpp
  src/AdaptedComposition.cpp
  src/StandardComposition.cpp
  src/AlignmentTraversor.cpp
  src/Ctm.cpp
  src/FstLoader.cpp
  src/KaldiArchiveFstLoader.cpp
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
  ${FST_KALDI_INCLUDES}
)

target_link_libraries(fstalign
  fstaligner-common
  ${CMAKE_DL_LIBS}
  ${FSTALIGN_LIBRARIES}
  ${FST_KALDI_LIBRARIES}
)

add_subdirectory(test)
