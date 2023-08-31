add_executable(test_util test_util.cpp ${PROJECT_SOURCE_DIR}/src/util.cpp)
target_include_directories(test_util PRIVATE ${PROJECT_SOURCE_DIR}/src)
add_compile_definitions(test_util PRIVATE TRACING)
target_link_libraries(test_util PRIVATE fmt::fmt argparse::argparse)