cmake_minimum_required(VERSION 3.25)
project(io-multiplexing)

if(NOT LINUX)
  message(FATAL "project ${PROJECT_NAME} must run on LINUX!")
endif()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(FetchContent)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        10.1.0
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG        v2.9
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)

set(ARGPARSE_BUILD_SAMPLES FALSE)
set(ARGPARSE_BUILD_TESTS FALSE)
FetchContent_MakeAvailable(argparse)

include(add-antlr4)

add_antlr4_jar(4.13.0)
add_antlr4_cpp(4.13.0 STATIC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(src)
add_subdirectory(test)