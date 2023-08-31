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

FetchContent_Declare(
  antlr4_jar
  URL           https://www.antlr.org/download/antlr-4.13.0-complete.jar
  DOWNLOAD_DIR  ${PROJECT_SOURCE_DIR}/jar
  DOWNLOAD_NAME antlr4-complete.jar
  DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_Populate(antlr4_jar)
set(ANTLR_EXECUTABLE ${PROJECT_SOURCE_DIR}/jar/antlr4-complete.jar)

include(add-antlr4-parser)
include(add-antlr4)

add_antlr4(4.13.0 STATIC)

add_antlr4_parser(
  CalculatorParser src/Calculator.g4 
  LEXER PARSER 
  NAMESPACE parser
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_library(calculator_parser STATIC 
  ${ANTLR_CalculatorParser_CXX_OUTPUTS})
target_include_directories(calculator_parser PUBLIC ${ANTLR_CalculatorParser_OUTPUT_DIR})
target_link_libraries(calculator_parser PUBLIC antlr4::antlr4_static)
add_library(parser::calculator_parser ALIAS calculator_parser)

add_executable(test_linger src/test_linger.cpp)
target_sources(test_linger PRIVATE
  src/util.cpp
)
target_link_libraries(test_linger PRIVATE 
  fmt::fmt 
  argparse::argparse 
)
target_include_directories(test_linger PRIVATE src)
target_compile_definitions(test_linger PRIVATE TRACING)

add_executable(test_sticky_packet src/test_sticky_packet.cpp)
target_sources(test_sticky_packet PRIVATE
  src/util.cpp
  src/sync_calculator/responser.cpp 
  src/sync_calculator/requester.cpp
)
target_include_directories(test_sticky_packet PRIVATE 
  src
  src/sync_calculator
)
target_link_libraries(test_sticky_packet PRIVATE 
  fmt::fmt 
  argparse::argparse 
  parser::calculator_parser
)
target_compile_definitions(test_sticky_packet PRIVATE TRACING)

add_executable(benchmark_calculator src/benchmark.cpp)
target_sources(benchmark_calculator PRIVATE
  src/util.cpp
  src/sync_calculator/requester.cpp
)
target_include_directories(benchmark_calculator PRIVATE 
  src
  src/sync_calculator
)
target_link_libraries(benchmark_calculator PRIVATE 
  fmt::fmt 
  argparse::argparse 
  parser::calculator_parser
)
target_compile_definitions(benchmark_calculator PRIVATE TRACING)

add_executable(tcp_forward src/tcp_forward.cpp)

add_executable(test_OOB src/test_OOB.cpp)

add_subdirectory(src/server_type)

add_subdirectory(test)