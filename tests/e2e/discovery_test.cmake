file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/.git")
file(MAKE_DIRECTORY "${TEST_ROOT}/packages/widget")
file(
  WRITE
  "${TEST_ROOT}/.fs-lintrc"
  "{\"version\":1,\"newFiles\":{\"default\":\"deny\"}}"
)

execute_process(
  COMMAND "${FS_LINT}" check-path src/new-helper.c
  WORKING_DIRECTORY "${TEST_ROOT}/packages/widget"
  RESULT_VARIABLE status
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

if(NOT status EQUAL 1)
  message(FATAL_ERROR "expected exit code 1, received ${status}: ${output}${error}")
endif()

set(conflict_root "${TEST_ROOT}/conflict")
file(MAKE_DIRECTORY "${conflict_root}")
file(WRITE "${conflict_root}/.fs-lintrc" "{\"version\":1,\"newFiles\":{\"default\":\"deny\"}}")
file(WRITE "${conflict_root}/fs-lint.json" "{\"version\":1,\"newFiles\":{\"default\":\"deny\"}}")

execute_process(
  COMMAND "${FS_LINT}" check-path src/new-helper.c
  WORKING_DIRECTORY "${conflict_root}"
  RESULT_VARIABLE conflict_status
  OUTPUT_VARIABLE conflict_output
)

if(NOT conflict_status EQUAL 2)
  message(FATAL_ERROR "expected conflicting configs to fail: ${conflict_output}")
endif()

if(NOT conflict_output MATCHES "multiple configuration files found")
  message(FATAL_ERROR "unexpected conflict output: ${conflict_output}")
endif()
