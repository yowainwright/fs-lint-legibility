#include "cli_output.h"
#include "config.h"
#include "legibility.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  const char *root;
  const char *config_path;
  const char *path;
  cli_output_format format;
} cli_arguments;

static int usage(void) {
  fputs("usage: fs-lint check-path [--root path] [--config path] "
        "[--format text|json] <path>\n",
        stderr);
  return LEGIBILITY_STATUS_ERROR;
}

static void initialize_arguments(cli_arguments *arguments) {
  *arguments = (cli_arguments){
      .root = ".",
      .config_path = NULL,
      .path = NULL,
      .format = CLI_OUTPUT_TEXT,
  };
}

static const char *next_value(int argc, char **argv, size_t *index) {
  *index += 1;
  if (*index >= (size_t)argc) {
    return NULL;
  }
  const char *value = argv[*index];
  *index += 1;
  return value;
}

static int read_format(int argc, char **argv, size_t *index, cli_arguments *args) {
  const char *value = next_value(argc, argv, index);
  if (value == NULL) {
    return 0;
  }
  if (strcmp(value, "json") == 0) {
    args->format = CLI_OUTPUT_JSON;
    return 1;
  }
  if (strcmp(value, "text") == 0) {
    args->format = CLI_OUTPUT_TEXT;
    return 1;
  }
  return 0;
}

static int read_string_option(int argc, char **argv, size_t *index,
                              const char **destination) {
  const char *value = next_value(argc, argv, index);
  if (value == NULL) {
    return 0;
  }
  *destination = value;
  return 1;
}

static int parse_option(int argc, char **argv, size_t *index,
                        cli_arguments *arguments) {
  const char *option = argv[*index];
  if (strcmp(option, "--root") == 0) {
    return read_string_option(argc, argv, index, &arguments->root);
  }
  if (strcmp(option, "--config") == 0) {
    return read_string_option(argc, argv, index, &arguments->config_path);
  }
  if (strcmp(option, "--format") == 0) {
    return read_format(argc, argv, index, arguments);
  }
  return 0;
}

static int assign_path(const char *path, size_t *index, cli_arguments *arguments) {
  if (arguments->path != NULL) {
    return 0;
  }
  arguments->path = path;
  *index += 1;
  return 1;
}

static int parse_token(int argc, char **argv, size_t *index, cli_arguments *arguments) {
  const bool is_option = argv[*index][0] == '-';
  if (is_option) {
    return parse_option(argc, argv, index, arguments);
  }
  return assign_path(argv[*index], index, arguments);
}

static int parse_arguments(int argc, char **argv, cli_arguments *arguments) {
  initialize_arguments(arguments);
  const bool valid_command = argc >= 2 && strcmp(argv[1], "check-path") == 0;
  if (!valid_command) {
    return 0;
  }
  size_t index = 2;
  while (index < (size_t)argc) {
    if (!parse_token(argc, argv, &index, arguments)) {
      return 0;
    }
  }
  return arguments->path != NULL;
}

static void report_config_error(const cli_config *config, cli_output *output) {
  const legibility_diagnostic diagnostic = {
      .severity = LEGIBILITY_SEVERITY_ERROR,
      .code = "config/invalid",
      .path = config->source_path,
      .message = config->error,
  };
  cli_report(&diagnostic, output);
}

static int check_path(const cli_arguments *arguments) {
  cli_output output = {.format = arguments->format, .stream = stdout};
  cli_config config;
  const bool loaded = cli_config_load(arguments->root, arguments->config_path, &config);
  if (!loaded) {
    report_config_error(&config, &output);
    cli_config_destroy(&config);
    return LEGIBILITY_STATUS_ERROR;
  }
  const legibility_change change = {
      .path = arguments->path,
      .kind = LEGIBILITY_CHANGE_ADDED,
  };
  const legibility_status status =
      legibility_check(&config.policy, &change, 1, cli_report, &output);
  cli_config_destroy(&config);
  return (int)status;
}

int main(int argc, char **argv) {
  cli_arguments arguments;
  if (!parse_arguments(argc, argv, &arguments)) {
    return usage();
  }
  return check_path(&arguments);
}
