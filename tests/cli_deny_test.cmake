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
