#include "legibility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  size_t count;
  char code[64];
  char path[256];
} captured_diagnostics;

static void fail(const char *message) {
  fprintf(stderr, "%s\n", message);
  exit(EXIT_FAILURE);
}

static void capture(const legibility_diagnostic *diagnostic, void *user_data) {
  captured_diagnostics *captured = user_data;
  captured->count += 1;
  snprintf(captured->code, sizeof(captured->code), "%s", diagnostic->code);
  snprintf(captured->path, sizeof(captured->path), "%s", diagnostic->path);
}

static legibility_status check(const legibility_config *config, const char *path,
                               captured_diagnostics *captured) {
  const legibility_change change = {
      .path = path,
      .kind = LEGIBILITY_CHANGE_ADDED,
  };
  return legibility_check(config, &change, 1, capture, captured);
}

static void test_denies_added_file(void) {
  const legibility_config config = {
      .new_files_default = LEGIBILITY_NEW_FILES_DENY,
  };
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, "src/new-helper.c", &captured);
  const int denied = status == LEGIBILITY_STATUS_VIOLATIONS;
  const int correct_code = strcmp(captured.code, "files/new") == 0;
  const int correct_path = strcmp(captured.path, "src/new-helper.c") == 0;
  if (!denied || captured.count != 1 || !correct_code || !correct_path) {
    fail("expected an added file violation");
  }
}

static void test_defaults_to_deny(void) {
  const legibility_config config = {0};
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, "src/new-helper.c", &captured);
  const int denied = status == LEGIBILITY_STATUS_VIOLATIONS;
  if (!denied || strcmp(captured.code, "files/new") != 0) {
    fail("expected a zero-initialized configuration to deny added files");
  }
}

static void test_allows_established_pattern(void) {
  const char *allow_patterns[] = {"src/**/index.c"};
  const legibility_config config = {
      .new_files_default = LEGIBILITY_NEW_FILES_DENY,
      .allow_patterns = allow_patterns,
      .allow_pattern_count = 1,
  };
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, "src/widget/index.c", &captured);
  const int allowed = status == LEGIBILITY_STATUS_OK && captured.count == 0;
  if (!allowed) {
    fail("expected the allow pattern to permit the added path");
  }
}

static void test_allows_globstar_without_directory(void) {
  const char *allow_patterns[] = {"src/**/index.c"};
  const legibility_config config = {
      .allow_patterns = allow_patterns,
      .allow_pattern_count = 1,
  };
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, "src/index.c", &captured);
  if (status != LEGIBILITY_STATUS_OK || captured.count != 0) {
    fail("expected globstar directory to match zero directories");
  }
}

static void test_star_does_not_cross_directory(void) {
  const char *allow_patterns[] = {"src/*.c"};
  const legibility_config config = {
      .allow_patterns = allow_patterns,
      .allow_pattern_count = 1,
  };
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, "src/nested/file.c", &captured);
  if (status != LEGIBILITY_STATUS_VIOLATIONS || captured.count != 1) {
    fail("expected star to stay within one path segment");
  }
}

static void test_rejects_missing_config(void) {
  captured_diagnostics captured = {0};
  const legibility_status status = legibility_check(NULL, NULL, 0, capture, &captured);
  const int rejected = status == LEGIBILITY_STATUS_ERROR;
  const int correct_code = strcmp(captured.code, "input/invalid") == 0;
  if (!rejected || captured.count != 1 || !correct_code) {
    fail("expected invalid library input to return an error diagnostic");
  }
}

static void test_rejects_missing_changes(void) {
  const legibility_config config = {
      .new_files_default = LEGIBILITY_NEW_FILES_DENY,
  };
  captured_diagnostics captured = {0};
  const legibility_status status =
      legibility_check(&config, NULL, 1, capture, &captured);
  const int rejected = status == LEGIBILITY_STATUS_ERROR;
  if (!rejected || strcmp(captured.code, "input/invalid") != 0) {
    fail("expected a missing change array to return an error diagnostic");
  }
}

static void test_rejects_missing_path(void) {
  const legibility_config config = {
      .new_files_default = LEGIBILITY_NEW_FILES_DENY,
  };
  const legibility_change change = {
      .path = NULL,
      .kind = LEGIBILITY_CHANGE_ADDED,
  };
  captured_diagnostics captured = {0};
  const legibility_status status =
      legibility_check(&config, &change, 1, capture, &captured);
  const int rejected = status == LEGIBILITY_STATUS_ERROR;
  if (!rejected || strcmp(captured.code, "input/invalid") != 0) {
    fail("expected a missing path to return an error diagnostic");
  }
}

static void test_rejects_missing_allow_patterns(void) {
  const legibility_config config = {
      .new_files_default = LEGIBILITY_NEW_FILES_DENY,
      .allow_patterns = NULL,
      .allow_pattern_count = 1,
  };
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, "src/new-helper.c", &captured);
  const int rejected = status == LEGIBILITY_STATUS_ERROR;
  if (!rejected || strcmp(captured.code, "input/invalid") != 0) {
    fail("expected missing allow patterns to return an error diagnostic");
  }
}

static void test_rejects_missing_allow_pattern(void) {
  const char *allow_patterns[] = {NULL};
  const legibility_config config = {
      .new_files_default = LEGIBILITY_NEW_FILES_DENY,
      .allow_patterns = allow_patterns,
      .allow_pattern_count = 1,
  };
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, "src/new-helper.c", &captured);
  const int rejected = status == LEGIBILITY_STATUS_ERROR;
  if (!rejected || strcmp(captured.code, "input/invalid") != 0) {
    fail("expected a missing allow pattern to return an error diagnostic");
  }
}

static void test_rejects_invalid_default(void) {
  const legibility_config config = {
      .new_files_default = (legibility_new_files_default)99,
  };
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, "src/new-helper.c", &captured);
  const int rejected = status == LEGIBILITY_STATUS_ERROR;
  if (!rejected || strcmp(captured.code, "input/invalid") != 0) {
    fail("expected an invalid new-file default to return an error diagnostic");
  }
}

static void test_rejects_invalid_change_kind(void) {
  const legibility_config config = {0};
  const legibility_change change = {
      .path = "src/new-helper.c",
      .kind = (legibility_change_kind)99,
  };
  captured_diagnostics captured = {0};
  const legibility_status status =
      legibility_check(&config, &change, 1, capture, &captured);
  const int rejected = status == LEGIBILITY_STATUS_ERROR;
  if (!rejected || strcmp(captured.code, "input/invalid") != 0) {
    fail("expected an invalid change kind to return an error diagnostic");
  }
}

static void test_rejects_oversized_path(void) {
  char path[LEGIBILITY_MAX_PATH_LENGTH + 2];
  memset(path, 'a', sizeof(path) - 1);
  path[sizeof(path) - 1] = '\0';
  const legibility_config config = {0};
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, path, &captured);
  const int rejected = status == LEGIBILITY_STATUS_ERROR;
  if (!rejected || strcmp(captured.code, "input/invalid") != 0) {
    fail("expected an oversized path to return an error diagnostic");
  }
}

static void test_rejects_oversized_pattern(void) {
  char pattern[LEGIBILITY_MAX_PATTERN_LENGTH + 2];
  memset(pattern, 'a', sizeof(pattern) - 1);
  pattern[sizeof(pattern) - 1] = '\0';
  const char *allow_patterns[] = {pattern};
  const legibility_config config = {
      .allow_patterns = allow_patterns,
      .allow_pattern_count = 1,
  };
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, "src/new-helper.c", &captured);
  const int rejected = status == LEGIBILITY_STATUS_ERROR;
  if (!rejected || strcmp(captured.code, "input/invalid") != 0) {
    fail("expected an oversized pattern to return an error diagnostic");
  }
}

static void test_allows_maximum_path_with_globstar(void) {
  char path[LEGIBILITY_MAX_PATH_LENGTH + 1];
  memset(path, 'a', sizeof(path) - 1);
  path[sizeof(path) - 1] = '\0';
  const char *allow_patterns[] = {"**"};
  const legibility_config config = {
      .allow_patterns = allow_patterns,
      .allow_pattern_count = 1,
  };
  captured_diagnostics captured = {0};
  const legibility_status status = check(&config, path, &captured);
  if (status != LEGIBILITY_STATUS_OK || captured.count != 0) {
    fail("expected globstar to allow a maximum-length path");
  }
}

int main(void) {
  test_denies_added_file();
  test_defaults_to_deny();
  test_allows_established_pattern();
  test_allows_globstar_without_directory();
  test_star_does_not_cross_directory();
  test_rejects_missing_config();
  test_rejects_missing_changes();
  test_rejects_missing_path();
  test_rejects_missing_allow_patterns();
  test_rejects_missing_allow_pattern();
  test_rejects_invalid_default();
  test_rejects_invalid_change_kind();
  test_rejects_oversized_path();
  test_rejects_oversized_pattern();
  test_allows_maximum_path_with_globstar();
  return EXIT_SUCCESS;
}
