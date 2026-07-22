file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
file(
  WRITE
  "${TEST_ROOT}/.legibilityrc.json"
  "{\"version\":1,\"newFiles\":{}}"
)

string(
  CONCAT
  command
  "printf 'src/new helper.c\\000tests/new-helper.test.c\\000' | "
  "\"$1\" check --stdin0 --root \"$2\""
)

execute_process(
  COMMAND /bin/sh -c "${command}" fs-lint-e2e "${FS_LINT}" "${TEST_ROOT}"
  RESULT_VARIABLE status
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

string(
  CONCAT
  expected
  "src/new helper.c: error files/new: new file is not allowed by configuration\n"
  "tests/new-helper.test.c: error files/new: new file is not allowed by configuration\n"
)

if(NOT status EQUAL 1)
  message(FATAL_ERROR "expected batch violation exit code 1: ${error}")
endif()

if(NOT output STREQUAL expected)
  message(FATAL_ERROR "unexpected batch output: ${output}")
endif()

if(NOT error STREQUAL "")
  message(FATAL_ERROR "expected empty batch stderr: ${error}")
endif()

string(
  CONCAT
  control_command
  "printf 'src/line\\nbreak.c\\000' | "
  "\"$1\" check --stdin0 --root \"$2\""
)

execute_process(
  COMMAND /bin/sh -c "${control_command}" fs-lint-e2e "${FS_LINT}" "${TEST_ROOT}"
  RESULT_VARIABLE control_status
  OUTPUT_VARIABLE control_output
  ERROR_VARIABLE control_error
)

set(
  expected_control
  "src/line\\nbreak.c: error files/new: new file is not allowed by configuration\n"
)

if(NOT control_status EQUAL 1)
  message(FATAL_ERROR "expected control-character violation: ${control_error}")
endif()

if(NOT control_output STREQUAL expected_control)
  message(FATAL_ERROR "unexpected escaped text output: ${control_output}")
endif()

if(NOT control_error STREQUAL "")
  message(FATAL_ERROR "expected empty control-character stderr: ${control_error}")
endif()

string(
  CONCAT
  json_command
  "printf 'src/line\\nbreak.c\\000' | "
  "\"$1\" check --stdin0 --root \"$2\" --format json"
)

execute_process(
  COMMAND /bin/sh -c "${json_command}" fs-lint-e2e "${FS_LINT}" "${TEST_ROOT}"
  RESULT_VARIABLE json_status
  OUTPUT_VARIABLE json_output
  ERROR_VARIABLE json_error
)

string(
  CONCAT
  expected_json
  "{\"severity\":\"error\","
  "\"code\":\"files/new\","
  "\"path\":\"src/line\\nbreak.c\","
  "\"message\":\"new file is not allowed by configuration\"}\n"
)

if(NOT json_status EQUAL 1)
  message(FATAL_ERROR "expected JSON batch violation: ${json_error}")
endif()

if(NOT json_output STREQUAL expected_json)
  message(FATAL_ERROR "unexpected JSON batch output: ${json_output}")
endif()

if(NOT json_error STREQUAL "")
  message(FATAL_ERROR "expected empty JSON batch stderr: ${json_error}")
endif()

string(
  CONCAT
  invalid_utf8_command
  "printf 'src/\\377.c\\000' | "
  "\"$1\" check --stdin0 --root \"$2\" --format json"
)

execute_process(
  COMMAND
    /bin/sh -c "${invalid_utf8_command}" fs-lint-e2e "${FS_LINT}" "${TEST_ROOT}"
  RESULT_VARIABLE invalid_utf8_status
  OUTPUT_VARIABLE invalid_utf8_output
  ERROR_VARIABLE invalid_utf8_error
  ENCODING NONE
)

string(
  CONCAT
  expected_invalid_utf8
  "{\"severity\":\"error\","
  "\"code\":\"files/new\","
  "\"path\":\"src/\\u00ff.c\","
  "\"message\":\"new file is not allowed by configuration\"}\n"
)

if(NOT invalid_utf8_status EQUAL 1)
  message(FATAL_ERROR "expected invalid UTF-8 filename violation")
endif()

if(NOT invalid_utf8_output STREQUAL expected_invalid_utf8)
  message(FATAL_ERROR "unexpected invalid UTF-8 JSON output: ${invalid_utf8_output}")
endif()

if(NOT invalid_utf8_error STREQUAL "")
  message(FATAL_ERROR "expected empty invalid UTF-8 stderr: ${invalid_utf8_error}")
endif()

string(
  CONCAT
  invalid_command
  "printf 'src/unterminated.c' | "
  "\"$1\" check --stdin0 --root \"$2\" --format json"
)

execute_process(
  COMMAND /bin/sh -c "${invalid_command}" fs-lint-e2e "${FS_LINT}" "${TEST_ROOT}"
  RESULT_VARIABLE invalid_status
  OUTPUT_VARIABLE invalid_output
  ERROR_VARIABLE invalid_error
)

string(
  CONCAT
  expected_invalid
  "{\"severity\":\"error\","
  "\"code\":\"input/invalid\","
  "\"path\":\"\","
  "\"message\":\"NUL-delimited input must end with NUL\"}\n"
)

if(NOT invalid_status EQUAL 2)
  message(FATAL_ERROR "expected invalid batch exit code 2")
endif()

if(NOT invalid_output STREQUAL expected_invalid)
  message(FATAL_ERROR "unexpected invalid batch output: ${invalid_output}")
endif()

if(NOT invalid_error STREQUAL "")
  message(FATAL_ERROR "expected empty invalid batch stderr: ${invalid_error}")
endif()
