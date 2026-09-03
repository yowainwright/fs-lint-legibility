file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
file(
  WRITE
  "${TEST_ROOT}/policy.json"
  "{\"version\":1,\"newFiles\":{\"default\":\"deny\"}}"
)
file(
  WRITE
  "${TEST_ROOT}/policy.toml"
  "version = 1\n[newFiles]\ndefault = \"allow\"\n"
)

execute_process(
  COMMAND
    "${FS_LINT}" check-path
    --root "${TEST_ROOT}"
    --config policy.json
    src/new-helper.c
  RESULT_VARIABLE status
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

if(NOT status EQUAL 1)
  message(FATAL_ERROR "expected exit code 1, received ${status}: ${output}${error}")
endif()

execute_process(
  COMMAND
    "${FS_LINT}" check-path
    --root "${TEST_ROOT}"
    --config policy.toml
    src/new-helper.c
  RESULT_VARIABLE toml_status
  OUTPUT_VARIABLE toml_output
  ERROR_VARIABLE toml_error
)

if(NOT toml_status EQUAL 0)
  message(FATAL_ERROR "expected TOML config to allow path: ${toml_output}${toml_error}")
endif()
