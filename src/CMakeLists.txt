add_antlr4_target(
  CalculatorParser 
  INPUT_FILES Calculator.g4 
  NAMESPACE parser
)

add_library(calculator_parser STATIC 
  ${antlr4_CalculatorParser_SOURCES})
target_include_directories(calculator_parser PUBLIC ${antlr4_CalculatorParser_INCLUDE_DIR})
target_link_libraries(calculator_parser PUBLIC antlr4::antlr4_static)
add_library(parser::calculator_parser ALIAS calculator_parser)



function(add_run_target TARGET_NAME SOURCE)
  set(UTIL_DIR
    ${PROJECT_SOURCE_DIR}/src/utils
  )

  set(UTIL_SOURCES
    ${UTIL_DIR}/common.cpp
    ${UTIL_DIR}/client.cpp
    ${UTIL_DIR}/server.cpp
  )

  set(SERVICE_DIR
    ${PROJECT_SOURCE_DIR}/src/sync_calculator
  )

  set(SERVICE_SOURCES 
    ${SERVICE_DIR}/responser.cpp
    ${SERVICE_DIR}/requester.cpp
  )

  set(LIBRARIES 
    spdlog::spdlog
    argparse::argparse
    parser::calculator_parser
  )

  add_executable(${TARGET_NAME} ${SOURCE})
  target_sources(${TARGET_NAME} PRIVATE
    ${UTIL_SOURCES}
    ${SERVICE_SOURCES}
  )
  target_include_directories(${TARGET_NAME} PRIVATE 
    ${PROJECT_SOURCE_DIR}/src
  )
  target_link_libraries(${TARGET_NAME} PRIVATE 
    ${LIBRARIES}
  )
endfunction(add_run_target)


set(SERVERS_DIR servers)

# single thread synchronized server
add_run_target(st_sync_server ${SERVERS_DIR}/st_sync_server.cpp)
# single thread select based server
add_run_target(st_select_server ${SERVERS_DIR}/st_select_server.cpp)

add_run_target(benchmark_calculator benchmark.cpp)