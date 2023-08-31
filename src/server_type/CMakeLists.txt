macro(add_server SERVER_TYPE SOURCE)
  add_executable(${SERVER_TYPE} ${SOURCE})
  target_sources(${SERVER_TYPE} PRIVATE
    ${PROJECT_SOURCE_DIR}/src/util.cpp
    ${PROJECT_SOURCE_DIR}/src/sync_calculator/responser.cpp 
    ${PROJECT_SOURCE_DIR}/src/sync_calculator/requester.cpp
  )
  target_include_directories(${SERVER_TYPE} PRIVATE 
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/src/sync_calculator
  )
  target_link_libraries(${SERVER_TYPE} PRIVATE 
    fmt::fmt 
    argparse::argparse 
    parser::calculator_parser
  )
  target_compile_definitions(${SERVER_TYPE} PRIVATE TRACING)
endmacro(add_server)


# single thread synchronized server
add_server(st_sync_server st_sync_server.cpp)
# single thread select based server
add_server(st_select_server st_select_server.cpp)
