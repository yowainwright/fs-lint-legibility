file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
file(
  WRITE
  "${TEST_ROOT}/fs-lint.toml"
  "version = 1\n\n[newFiles]\ndefault = \"deny\"\nallow = [\"src/**/{index,utils,types,constants}.ts\", \"!**/*.generated.*\"]\n"
)

execute_process(
  COMMAND "${FS_LINT}" check-path --root "${TEST_ROOT}" src/auth/utils.ts
  RESULT_VARIABLE allowed_status
  OUTPUT_VARIABLE allowed_output
  ERROR_VARIABLE allowed_error
)

if(NOT allowed_status EQUAL 0)
  message(FATAL_ERROR "expected TOML config to allow path: ${allowed_output}${allowed_error}")
endif()

execute_process(
  COMMAND "${FS_LINT}" check-path --root "${TEST_ROOT}" src/auth/helper.ts
  RESULT_VARIABLE denied_status
  OUTPUT_VARIABLE denied_output
  ERROR_VARIABLE denied_error
)

set(
  expected_denied
  "src/auth/helper.ts: error files/new: new file is not allowed by configuration\n"
)
if(NOT denied_status EQUAL 1)
  message(FATAL_ERROR "expected TOML config to deny path: ${denied_output}${denied_error}")
endif()
if(NOT denied_output STREQUAL expected_denied)
  message(FATAL_ERROR "unexpected TOML denial output: ${denied_output}")
endif()
