function(add_antlr4 TAG)
  set(ANTLR_OPTIONS SHARED WITH_STATIC_CRT WITH_LIBCXX DISABLE_WARNINGS)
  set(ANTLR_ONE_VALUE_ARGS "")
  set(ANTLR_MULTI_VALUE_ARGS "")
  cmake_parse_arguments(ANTLR4CPP
                        "${ANTLR_OPTIONS}"
                        "${ANTLR_ONE_VALUE_ARGS}"
                        "${ANTLR_MULTI_VALUE_ARGS}"
                        ${ARGN})
  include(CMakePrintHelpers)
  FetchContent_Declare(
    antlr4_cpp
    GIT_REPOSITORY https://github.com/antlr/antlr4
    GIT_TAG ${TAG}
    GIT_SHALLOW TRUE
  )
  FetchContent_Populate(antlr4_cpp)

  set(ANTLR4CPP_RUNTIME_DIR 
    ${antlr4_cpp_SOURCE_DIR}/runtime/Cpp
    CACHE INTERNAL ""
  )


  # 可以从 VERSION 中读取到版本信息
  file(STRINGS ${ANTLR4CPP_RUNTIME_DIR}/VERSION ANTLR_VERSION)
  cmake_print_variables(ANTLR4CPP_RUNTIME_DIR)
  cmake_print_variables(ANTLR_VERSION)
  cmake_print_variables(ANTLR4CPP_WITH_STATIC_CRT)
  cmake_print_variables(ANTLR4CPP_WITH_LIBCXX)
  cmake_print_variables(ANTLR4CPP_DISABLE_WARNINGS)
  cmake_print_variables(ANTLR4CPP_SHARED)
  cmake_print_variables(ANTLR4_INCLUDE_DIRS)


  set(WITH_DEMO FALSE CACHE INTERNAL "")
  set(WITH_STATIC_CRT ${ANTLR4CPP_WITH_STATIC_CRT} CACHE INTERNAL "")
  set(WITH_LIBCXX ${ANTLR4CPP_WITH_LIBCXX} CACHE INTERNAL "")
  set(DISABLE_WARNINGS ${ANTLR4CPP_DISABLE_WARNINGS} CACHE INTERNAL "")
  set(ANTLR4_INSTALL FALSE CACHE INTERNAL "")
  set(ANTLR_BUILD_CPP_TESTS FALSE CACHE INTERNAL "")
  set(ANTLR4_INCLUDE_DIRS 
    ${ANTLR4CPP_RUNTIME_DIR}/runtime/src 
    CACHE INTERNAL "")
  set(ANTLR4_OUTPUT_DIR 
    ${antlr4_cpp_BINARY_DIR}/runtime 
    CACHE INTERNAL ""
  )

  if(ANTLR4CPP_SHARED)
    set(ANTLR_BUILD_SHARED TRUE CACHE INTERNAL "")
    set(ANTLR_BUILD_STATIC FALSE CACHE INTERNAL "")
  else()
    set(ANTLR_BUILD_SHARED FALSE CACHE INTERNAL "")
    set(ANTLR_BUILD_STATIC TRUE CACHE INTERNAL "")
  endif()

  # from https://github.com/antlr/antlr4/blob/dev/runtime/Cpp/cmake/ExternalAntlr4Cpp.cmake
  if(MSVC)
    set(ANTLR4_STATIC_LIBRARIES
      ${ANTLR4_OUTPUT_DIR}/antlr4-runtime-static.lib CACHE INTERNAL "")
    set(ANTLR4_SHARED_LIBRARIES
      ${ANTLR4_OUTPUT_DIR}/antlr4-runtime.lib CACHE INTERNAL "")
    set(ANTLR4_RUNTIME_LIBRARIES
      ${ANTLR4_OUTPUT_DIR}/antlr4-runtime.dll CACHE INTERNAL "")
  else()
    set(ANTLR4_STATIC_LIBRARIES
      ${ANTLR4_OUTPUT_DIR}/libantlr4-runtime.a CACHE INTERNAL "")
    if(MINGW)
      set(ANTLR4_SHARED_LIBRARIES
        ${ANTLR4_OUTPUT_DIR}/libantlr4-runtime.dll.a CACHE INTERNAL "")
      set(ANTLR4_RUNTIME_LIBRARIES
        ${ANTLR4_OUTPUT_DIR}/libantlr4-runtime.dll CACHE INTERNAL "")
    elseif(CYGWIN)
      set(ANTLR4_SHARED_LIBRARIES
        ${ANTLR4_OUTPUT_DIR}/libantlr4-runtime.dll.a CACHE INTERNAL "")
      set(ANTLR4_RUNTIME_LIBRARIES
        # https://github.com/antlr/antlr4/pull/2235#discussion_r173871830
        # due to cmake: https://cmake.org/cmake/help/latest/prop_tgt/SOVERSION.html
        ${ANTLR4_OUTPUT_DIR}/cygantlr4-runtime-${ANTLR_VERSION}.dll CACHE INTERNAL "")
    elseif(APPLE)
      set(ANTLR4_RUNTIME_LIBRARIES
        ${ANTLR4_OUTPUT_DIR}/libantlr4-runtime.dylib CACHE INTERNAL "")
    else()
      set(ANTLR4_RUNTIME_LIBRARIES
        ${ANTLR4_OUTPUT_DIR}/libantlr4-runtime.so CACHE INTERNAL "")
    endif()
  endif()

  add_subdirectory(
    ${ANTLR4CPP_RUNTIME_DIR} 
    ${antlr4_cpp_BINARY_DIR}
  )

  if(ANTLR4CPP_SHARED)
    add_library(antlr4_runtime_shared SHARED IMPORTED)
    target_include_directories(antlr4_runtime_shared
      INTERFACE ${ANTLR4_INCLUDE_DIRS}
    )

    set_property(TARGET antlr4_runtime_shared PROPERTY IMPORTED_LOCATION ${ANTLR4_RUNTIME_LIBRARIES})
    set_property(TARGET antlr4_runtime_shared PROPERTY IMPORTED_IMPLIB ${ANTLR4_SHARED_LIBRARIES})

    add_library(antlr4::antlr4_shared ALIAS antlr4_runtime_shared)
  else()
    target_include_directories(antlr4_static INTERFACE ${ANTLR4_INCLUDE_DIRS})
    add_library(antlr4::antlr4_static ALIAS antlr4_static)
  endif()

endfunction(add_antlr4)