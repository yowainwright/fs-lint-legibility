file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
file(
  WRITE
  "${TEST_ROOT}/.legibilityrc.json"
  "{\"version\":1,\"newFiles\":{}}"
)

execute_process(
  COMMAND "${FS_LINT}" check-path src/new-helper.c
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE status
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

set(
  expected
  "src/new-helper.c: error files/new: new file is not allowed by configuration\n"
)

if(NOT status EQUAL 1)
  message(FATAL_ERROR "expected exit code 1, received ${status}: ${error}")
endif()

if(NOT output STREQUAL expected)
  message(FATAL_ERROR "unexpected output: ${output}")
endif()

file(
  WRITE
  "${TEST_ROOT}/.legibilityrc.json"
  "{\"version\":1,\"newFiles\":{\"default\":\"allow\"}}"
)

execute_process(
  COMMAND "${FS_LINT}" check-path src/new-helper.c
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE allow_status
  OUTPUT_VARIABLE allow_output
)

if(NOT allow_status EQUAL 0 OR NOT allow_output STREQUAL "")
  message(FATAL_ERROR "expected explicit allow to exit cleanly: ${allow_output}")
endif()

string(
  CONCAT
  pattern_config
  "{\"version\":1,"
  "\"newFiles\":{\"default\":\"deny\","
  "\"allow\":[\"src/**/index.c\"]}}"
)
file(WRITE "${TEST_ROOT}/.legibilityrc.json" "${pattern_config}")

execute_process(
  COMMAND "${FS_LINT}" check-path src/widget/index.c
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE pattern_status
  OUTPUT_VARIABLE pattern_output
  ERROR_VARIABLE pattern_error
)

if(NOT pattern_status EQUAL 0)
  message(FATAL_ERROR "expected allow pattern to exit 0: ${pattern_error}")
endif()

if(NOT pattern_output STREQUAL "" OR NOT pattern_error STREQUAL "")
  message(FATAL_ERROR "expected allow pattern to produce no output")
endif()
