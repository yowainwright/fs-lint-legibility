cmake_minimum_required(VERSION 3.20)

project(fs_lint_legibility VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

function(legibility_enable_warnings target)
  if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(
      ${target}
      PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow
    )
  endif()
endfunction()

add_library(yyjson STATIC vendor/yyjson/yyjson.c)
target_include_directories(yyjson PUBLIC vendor/yyjson)

add_library(legibility STATIC src/glob.c src/legibility.c)
target_include_directories(
  legibility
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE src
)

legibility_enable_warnings(legibility)

add_executable(
  fs-lint
  src/cli_output.c
  src/config.c
  src/discover.c
  src/main.c
)
target_include_directories(fs-lint PRIVATE src)
target_link_libraries(fs-lint PRIVATE legibility yyjson)
legibility_enable_warnings(fs-lint)

install(
  TARGETS legibility fs-lint
  EXPORT legibility-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES include/legibility.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  EXPORT legibility-targets
  FILE legibility-targets.cmake
  NAMESPACE legibility::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/legibility
)

include(CTest)

if(BUILD_TESTING)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test-data)
  add_executable(legibility-test tests/legibility_test.c)
  target_link_libraries(legibility-test PRIVATE legibility)
  legibility_enable_warnings(legibility-test)
  add_test(NAME core-policy COMMAND legibility-test)
  add_executable(cli-output-test tests/cli_output_test.c src/cli_output.c)
  target_include_directories(cli-output-test PRIVATE src)
  target_link_libraries(cli-output-test PRIVATE legibility)
  legibility_enable_warnings(cli-output-test)
  add_test(NAME cli-output COMMAND cli-output-test)
  add_test(
    NAME cli-deny-new-file
    COMMAND
      ${CMAKE_COMMAND}
      -DFS_LINT=$<TARGET_FILE:fs-lint>
      -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-data/cli-deny
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_deny_test.cmake
  )
  add_test(
    NAME cli-discovers-parent-config
    COMMAND
      ${CMAKE_COMMAND}
      -DFS_LINT=$<TARGET_FILE:fs-lint>
      -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-data/cli-discovery
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_discovery_test.cmake
  )
  add_test(
    NAME cli-json-diagnostic
    COMMAND
      ${CMAKE_COMMAND}
      -DFS_LINT=$<TARGET_FILE:fs-lint>
      -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-data/cli-json
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_json_test.cmake
  )
  add_test(
    NAME cli-rejects-invalid-config-keys
    COMMAND
      ${CMAKE_COMMAND}
      -DFS_LINT=$<TARGET_FILE:fs-lint>
      -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-data/cli-unknown-key
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_invalid_keys_test.cmake
  )
  add_test(
    NAME cli-explicit-config
    COMMAND
      ${CMAKE_COMMAND}
      -DFS_LINT=$<TARGET_FILE:fs-lint>
      -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-data/cli-explicit
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_explicit_config_test.cmake
  )
endif()
