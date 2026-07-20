execute_process(
  COMMAND "${FS_LINT}"
  RESULT_VARIABLE status
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

string(
  CONCAT
  expected_error
  "usage: fs-lint check-path "
  "[--root path] [--config path] "
  "[--format text|json] <path>\n"
)

if(NOT status EQUAL 2)
  message(FATAL_ERROR "expected usage exit code 2, received ${status}")
endif()

if(NOT output STREQUAL "")
  message(FATAL_ERROR "expected empty stdout: ${output}")
endif()

if(NOT error STREQUAL expected_error)
  message(FATAL_ERROR "unexpected stderr: ${error}")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")

execute_process(
  COMMAND "${FS_LINT}" check-path --root "${TEST_ROOT}" src/new-helper.c
  RESULT_VARIABLE config_status
  OUTPUT_VARIABLE config_output
  ERROR_VARIABLE config_error
)

string(
  CONCAT
  expected_output
  "${TEST_ROOT}"
  ": error config/invalid: "
  "no configuration file found\n"
)

if(NOT config_status EQUAL 2)
  message(FATAL_ERROR "expected missing-config exit code 2")
endif()

if(NOT config_output STREQUAL expected_output)
  message(FATAL_ERROR "unexpected missing-config stdout: ${config_output}")
endif()

if(NOT config_error STREQUAL "")
  message(FATAL_ERROR "expected empty missing-config stderr: ${config_error}")
endif()
