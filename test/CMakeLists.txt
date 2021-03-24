cmake_minimum_required(VERSION 3.5)

find_package(Threads REQUIRED)

include_directories(
  ${CMAKE_INSTALL_PREFIX}/include
  ${FSTALIGN_INCLUDES}
  ${FST_KALDI_INCLUDES}
  ${PROJECT_SOURCE_DIR}/test
  ${PROJECT_SOURCE_DIR}
  ${CMAKE_DL_LIBS}
)

link_libraries(
  ${FST_KALDI_LIBRARIES}
  fstaligner-common
)

add_executable(fstalign_Test fstalign_Test.cc)
target_link_libraries(fstalign_Test Threads::Threads)

add_test(NAME fstalign_Test
  COMMAND $<TARGET_FILE:fstalign_Test>
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/build)

add_executable(compose-tests compose-tests.cc)
target_link_libraries(compose-tests Threads::Threads)

add_test(NAME compose-tests
  COMMAND $<TARGET_FILE:compose-tests>
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
