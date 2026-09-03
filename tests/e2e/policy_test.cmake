file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}")
file(
  WRITE
  "${TEST_ROOT}/fs-lint.json"
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

execute_process(
  COMMAND "${FS_LINT}" check-path -- -new.c
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE dash_path_status
  OUTPUT_VARIABLE dash_path_output
  ERROR_VARIABLE dash_path_error
)

set(
  expected_dash_path
  "-new.c: error files/new: new file is not allowed by configuration\n"
)

if(NOT dash_path_status EQUAL 1)
  message(FATAL_ERROR "expected dash-prefixed path exit code 1")
endif()

if(NOT dash_path_output STREQUAL expected_dash_path)
  message(FATAL_ERROR "unexpected dash-prefixed path output: ${dash_path_output}")
endif()

if(NOT dash_path_error STREQUAL "")
  message(FATAL_ERROR "expected empty dash-prefixed path stderr: ${dash_path_error}")
endif()

file(
  WRITE
  "${TEST_ROOT}/fs-lint.json"
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

execute_process(
  COMMAND "${FS_LINT}" check-path ../outside.c
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE relative_status
  OUTPUT_VARIABLE relative_output
  ERROR_VARIABLE relative_error
)

set(
  expected_relative_error
  ": error input/invalid: change path must be normalized and repository-relative\n"
)

if(NOT relative_status EQUAL 2)
  message(FATAL_ERROR "expected non-relative path exit code 2")
endif()

if(NOT relative_output STREQUAL expected_relative_error)
  message(FATAL_ERROR "unexpected non-relative path output: ${relative_output}")
endif()

if(NOT relative_error STREQUAL "")
  message(FATAL_ERROR "expected empty non-relative path stderr: ${relative_error}")
endif()

string(
  CONCAT
  pattern_config
  "{\"version\":1,"
  "\"newFiles\":{\"default\":\"deny\","
  "\"allow\":[\"src/**/index.c\"]}}"
)
file(WRITE "${TEST_ROOT}/fs-lint.json" "${pattern_config}")

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

string(
  CONCAT
  large_brace_config
  "{\"version\":1,"
  "\"newFiles\":{\"default\":\"deny\","
  "\"allow\":[\"src/{a,aa}{a,aa}{a,aa}{a,aa}{a,aa}{a,aa}{a,aa}{a,aa}{a,aa}{a,aa}{a,aa}{a,aa}{a,aa}.c\"]}}"
)
file(WRITE "${TEST_ROOT}/fs-lint.json" "${large_brace_config}")

execute_process(
  COMMAND "${FS_LINT}" check-path src/aaaaaaaaaaaaa.c
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE large_brace_status
  OUTPUT_VARIABLE large_brace_output
  ERROR_VARIABLE large_brace_error
)

if(NOT large_brace_status EQUAL 0)
  message(FATAL_ERROR "expected large brace product to exit 0: ${large_brace_error}")
endif()

if(NOT large_brace_output STREQUAL "" OR NOT large_brace_error STREQUAL "")
  message(FATAL_ERROR "expected large brace product to produce no output")
endif()

execute_process(
  COMMAND "${FS_LINT}" check-path src/aaaaaaaaaaaaa.txt
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE rejected_large_brace_status
  OUTPUT_VARIABLE rejected_large_brace_output
  ERROR_VARIABLE rejected_large_brace_error
)

if(NOT rejected_large_brace_status EQUAL 1)
  message(FATAL_ERROR "expected large brace nonmatch to exit 1")
endif()

if(NOT rejected_large_brace_error STREQUAL "")
  message(FATAL_ERROR "expected large brace nonmatch to leave stderr empty")
endif()

string(
  CONCAT
  wildcard_brace_config
  "{\"version\":1,"
  "\"newFiles\":{\"default\":\"deny\","
  "\"allow\":[\"src/{a*,aa*}{a*,aa*}{a*,aa*}{a*,aa*}{a*,aa*}{a*,aa*}{a*,aa*}{a*,aa*}.c\"]}}"
)
file(WRITE "${TEST_ROOT}/fs-lint.json" "${wildcard_brace_config}")

execute_process(
  COMMAND "${FS_LINT}" check-path src/aaaaaaaaaaaaa.txt
  WORKING_DIRECTORY "${TEST_ROOT}"
  RESULT_VARIABLE rejected_wildcard_brace_status
  OUTPUT_VARIABLE rejected_wildcard_brace_output
  ERROR_VARIABLE rejected_wildcard_brace_error
)

if(NOT rejected_wildcard_brace_status EQUAL 1)
  message(FATAL_ERROR "expected wildcard brace nonmatch to exit 1")
endif()

if(NOT rejected_wildcard_brace_error STREQUAL "")
  message(FATAL_ERROR "expected wildcard brace nonmatch to leave stderr empty")
endif()
