file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
file(REAL_PATH "${TEST_ROOT}" canonical_root)
file(
  WRITE
  "${TEST_ROOT}/fs-lint.json"
  "{\"version\":1,\"newFiles\":{\"default\":\"deny\"}}"
)

execute_process(
  COMMAND "${FS_LINT}" check-path --format json src/new-helper.c
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE status
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

string(
  CONCAT
  expected
  "{\"severity\":\"error\","
  "\"code\":\"files/new\","
  "\"path\":\"src/new-helper.c\","
  "\"message\":\"new file is not allowed by configuration\"}\n"
)

if(NOT status EQUAL 1)
  message(FATAL_ERROR "expected exit code 1, received ${status}: ${error}")
endif()

if(NOT output STREQUAL expected)
  message(FATAL_ERROR "unexpected output: ${output}")
endif()

if(NOT error STREQUAL "")
  message(FATAL_ERROR "expected empty stderr: ${error}")
endif()

file(
  WRITE
  "${TEST_ROOT}/fs-lint.json"
  "{\"version\":2,\"newFiles\":{}}"
)

execute_process(
  COMMAND "${FS_LINT}" check-path --format json src/new-helper.c
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE config_status
  OUTPUT_VARIABLE config_output
  ERROR_VARIABLE config_error
)

string(
  CONCAT
  expected_config_output
  "{\"severity\":\"error\","
  "\"code\":\"config/invalid\","
  "\"path\":\"${canonical_root}/fs-lint.json\","
  "\"message\":\"version must be 1\"}\n"
)

if(NOT config_status EQUAL 2)
  message(FATAL_ERROR "expected config error exit code 2")
endif()

if(NOT config_output STREQUAL expected_config_output)
  message(FATAL_ERROR "unexpected JSON config output: ${config_output}")
endif()

if(NOT config_error STREQUAL "")
  message(FATAL_ERROR "expected empty config stderr: ${config_error}")
endif()
