find_package(Java REQUIRED Runtime)

function(add_antlr4_parser TARGET_NAME INPUT_FILE)
  set(ANTLR_OPTIONS LEXER PARSER LISTENER VISITOR)
  set(ANTLR_ONE_VALUE_ARGS NAMESPACE OUTPUT_DIRECTORY)
  set(ANTLR_MULTI_VALUE_ARGS GENERATE_FLAGS)
  cmake_parse_arguments(ANTLR_TARGET
                        "${ANTLR_OPTIONS}"
                        "${ANTLR_ONE_VALUE_ARGS}"
                        "${ANTLR_MULTI_VALUE_ARGS}"
                        ${ARGN})
  include(CMakePrintHelpers)
  set(ANTLR_${TARGET_NAME}_INPUT ${INPUT_FILE})

  cmake_path(GET INPUT_FILE STEM ANTLR_INPUT)

  if(ANTLR_TARGET_OUTPUT_DIRECTORY)
    set(ANTLR_${TARGET_NAME}_OUTPUT_DIR ${ANTLR_TARGET_OUTPUT_DIRECTORY})
  else()
    set(ANTLR_${TARGET_NAME}_OUTPUT_DIR
        ${CMAKE_CURRENT_BINARY_DIR}/antlr4cpp_generated_src/${ANTLR_INPUT})
  endif()
  # https://github.com/antlr/antlr4/issues/3138
  # FIXME: due to antlr4's inconsistense behavior under linux and windows (windows output directory is output_directory, while linux is output_directory / src)
  # we should use cmake to find the actual output directory and move it to output_directory
  set(SRC_OUTPUT_DIR ${ANTLR_${TARGET_NAME}_OUTPUT_DIR}/src)

  unset(ANTLR_${TARGET_NAME}_CXX_OUTPUTS)

  if((ANTLR_TARGET_LEXER AND NOT ANTLR_TARGET_PARSER) OR
      (ANTLR_TARGET_PARSER AND NOT ANTLR_TARGET_LEXER))
    list(APPEND ANTLR_${TARGET_NAME}_CXX_OUTPUTS
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}.h
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}.cpp)
    set(ANTLR_${TARGET_NAME}_OUTPUTS
        ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}.interp
        ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}.tokens)
  else()
    list(APPEND ANTLR_${TARGET_NAME}_CXX_OUTPUTS
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}Lexer.h
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}Lexer.cpp
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}Parser.h
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}Parser.cpp)
    list(APPEND ANTLR_${TARGET_NAME}_OUTPUTS
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}Lexer.interp
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}Lexer.tokens)
  endif()

  if(ANTLR_TARGET_LISTENER)
    list(APPEND ANTLR_${TARGET_NAME}_CXX_OUTPUTS
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}BaseListener.h
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}BaseListener.cpp
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}Listener.h
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}Listener.cpp)
    list(APPEND ANTLR_TARGET_GENERATE_FLAGS -listener)
  endif()

  if(ANTLR_TARGET_VISITOR)
    list(APPEND ANTLR_${TARGET_NAME}_CXX_OUTPUTS
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}BaseVisitor.h
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}BaseVisitor.cpp
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}Visitor.h
          ${SRC_OUTPUT_DIR}/${ANTLR_INPUT}Visitor.cpp)
    list(APPEND ANTLR_TARGET_GENERATE_FLAGS -visitor)
  endif()

  if(ANTLR_TARGET_NAMESPACE)
    list(APPEND ANTLR_TARGET_GENERATE_FLAGS -package ${ANTLR_TARGET_NAMESPACE})
  endif()

  list(APPEND ANTLR_${TARGET_NAME}_OUTPUTS ${ANTLR_${TARGET_NAME}_CXX_OUTPUTS})

  execute_process(
      COMMAND ${Java_JAVA_EXECUTABLE} -jar ${ANTLR_EXECUTABLE}
              ${INPUT_FILE}
              -o ${ANTLR_${TARGET_NAME}_OUTPUT_DIR}
              -no-listener
              -Dlanguage=Cpp
              ${ANTLR_TARGET_GENERATE_FLAGS}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )
  set(ANTLR_${TARGET_NAME}_OUTPUT_DIR ${SRC_OUTPUT_DIR})
  cmake_policy(SET CMP0140 NEW)
  return(PROPAGATE 
    ANTLR_${TARGET_NAME}_OUTPUT_DIR 
    ANTLR_${TARGET_NAME}_CXX_OUTPUTS 
    ANTLR_${TARGET_NAME}_OUTPUTS
  )
endfunction(add_antlr4_parser)