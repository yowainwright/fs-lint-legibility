function(assert_invalid_config case_name filename contents expected_message)
  set(case_root "${TEST_ROOT}/${case_name}")
  file(REMOVE_RECURSE "${case_root}")
  file(MAKE_DIRECTORY "${case_root}")
  file(WRITE "${case_root}/${filename}" "${contents}")

  execute_process(
    COMMAND "${FS_LINT}" check-path src/new-helper.c
    WORKING_DIRECTORY "${case_root}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
  )

  if(NOT status EQUAL 2)
    message(FATAL_ERROR "expected exit code 2, received ${status}: ${output}")
  endif()
  if(NOT output MATCHES "${expected_message}")
    message(FATAL_ERROR "unexpected output: ${output}")
  endif()
endfunction()

assert_invalid_config(
  unknown
  .legibilityrc.json
  "{\"version\":1,\"newFiles\":{\"default\":\"deny\"},\"mystery\":true}"
  "unknown configuration key: mystery"
)

assert_invalid_config(
  duplicate
  .legibilityrc.json
  "{\"version\":1,\"version\":1,\"newFiles\":{\"default\":\"deny\"}}"
  "duplicate configuration key: version"
)

assert_invalid_config(
  toml
  .legibilityrc.toml
  "version = 1\n[newFiles]\ndefault = \"deny\"\n"
  "TOML configuration reader is not available yet"
)
