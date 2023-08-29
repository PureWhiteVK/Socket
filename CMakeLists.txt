cmake_minimum_required(VERSION 3.25)
project(io-multiplexing)

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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_library(util STATIC util.cpp)
target_include_directories(util PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(util PUBLIC fmt::fmt argparse::argparse)
add_library(util::util ALIAS util)

add_executable(test_linger test_linger.cpp)
target_link_libraries(test_linger PRIVATE util::util fmt::fmt argparse::argparse)