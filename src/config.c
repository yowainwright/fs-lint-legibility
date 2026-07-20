#include "config.h"

#include "discover.h"
#include "yyjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *name;
  bool seen;
} expected_key;

static char *copy_string(const char *value) {
  const size_t size = strlen(value) + 1;
  char *copy = malloc(size);
  if (copy != NULL) {
    memcpy(copy, value, size);
  }
  return copy;
}

static char *join_path(const char *root, const char *name) {
  const size_t size = strlen(root) + strlen(name) + 2;
  char *path = malloc(size);
  if (path != NULL) {
    snprintf(path, size, "%s/%s", root, name);
  }
  return path;
}

static char *resolve_config_path(const char *root, const char *config_path) {
  if (config_path[0] == '/') {
    return copy_string(config_path);
  }
  return join_path(root, config_path);
}

static bool has_suffix(const char *value, const char *suffix) {
  const size_t value_length = strlen(value);
  const size_t suffix_length = strlen(suffix);
  const bool long_enough = value_length >= suffix_length;
  return long_enough && strcmp(value + value_length - suffix_length, suffix) == 0;
}

static const char *base_name(const char *path) {
  const char *separator = strrchr(path, '/');
  return separator == NULL ? path : separator + 1;
}

static const char *format_error(const char *path) {
  if (has_suffix(path, ".toml")) {
    return "TOML configuration reader is not available yet";
  }
  const bool yaml = has_suffix(path, ".yaml") || has_suffix(path, ".yml");
  if (yaml) {
    return "YAML configuration reader is not available yet";
  }
  const bool json =
      has_suffix(path, ".json") || strcmp(base_name(path), ".legibilityrc") == 0;
  if (!json) {
    return "configuration must use .legibilityrc or a supported extension";
  }
  return NULL;
}

static bool fail(cli_config *config, const char *message) {
  snprintf(config->error, sizeof(config->error), "%s", message);
  return false;
}

static size_t find_key(const char *name, const expected_key *keys, size_t key_count) {
  for (size_t index = 0; index < key_count; index += 1) {
    if (strcmp(name, keys[index].name) == 0) {
      return index;
    }
  }
  return key_count;
}

static bool record_key(const char *name, expected_key *keys, size_t key_count,
                       cli_config *config) {
  const size_t index = find_key(name, keys, key_count);
  if (index == key_count) {
    snprintf(config->error, sizeof(config->error), "unknown configuration key: %s",
             name);
    return false;
  }
  if (keys[index].seen) {
    snprintf(config->error, sizeof(config->error), "duplicate configuration key: %s",
             name);
    return false;
  }
  keys[index].seen = true;
  return true;
}

static bool validate_keys(yyjson_val *object, expected_key *keys, size_t key_count,
                          cli_config *config) {
  size_t index;
  size_t maximum;
  yyjson_val *key;
  yyjson_val *value;
  yyjson_obj_foreach(object, index, maximum, key, value) {
    (void)value;
    if (!record_key(yyjson_get_str(key), keys, key_count, config)) {
      return false;
    }
  }
  return true;
}

static bool validate_root_keys(yyjson_val *root, cli_config *config) {
  expected_key keys[] = {{"version", false}, {"newFiles", false}};
  return validate_keys(root, keys, 2, config);
}

static bool validate_new_file_keys(yyjson_val *new_files, cli_config *config) {
  expected_key keys[] = {{"default", false}, {"allow", false}};
  return validate_keys(new_files, keys, 2, config);
}

static bool read_version(yyjson_val *root, cli_config *config) {
  yyjson_val *version = yyjson_obj_get(root, "version");
  const bool supported = yyjson_is_uint(version) && yyjson_get_uint(version) == 1;
  if (!supported) {
    return fail(config, "version must be 1");
  }
  return true;
}

static bool read_default(yyjson_val *new_files, cli_config *config) {
  yyjson_val *value = yyjson_obj_get(new_files, "default");
  if (value == NULL) {
    return true;
  }
  const char *setting = yyjson_get_str(value);
  if (setting == NULL) {
    return fail(config, "newFiles.default must be \"allow\" or \"deny\"");
  }
  if (strcmp(setting, "deny") == 0) {
    config->policy.new_files_default = LEGIBILITY_NEW_FILES_DENY;
    return true;
  }
  if (strcmp(setting, "allow") == 0) {
    return true;
  }
  return fail(config, "newFiles.default must be \"allow\" or \"deny\"");
}

static bool allocate_patterns(size_t count, cli_config *config) {
  if (count == 0) {
    return true;
  }
  config->owned_allow_patterns = calloc(count, sizeof(*config->owned_allow_patterns));
  if (config->owned_allow_patterns == NULL) {
    return fail(config, "could not allocate allow patterns");
  }
  config->policy.allow_patterns = (const char *const *)config->owned_allow_patterns;
  config->policy.allow_pattern_count = count;
  return true;
}

static bool copy_pattern(yyjson_val *value, size_t index, cli_config *config) {
  const char *pattern = yyjson_get_str(value);
  if (pattern == NULL) {
    return fail(config, "newFiles.allow must contain only strings");
  }
  config->owned_allow_patterns[index] = copy_string(pattern);
  if (config->owned_allow_patterns[index] == NULL) {
    return fail(config, "could not allocate allow pattern");
  }
  return true;
}

static bool read_allow(yyjson_val *new_files, cli_config *config) {
  yyjson_val *allow = yyjson_obj_get(new_files, "allow");
  if (allow == NULL) {
    return true;
  }
  if (!yyjson_is_arr(allow)) {
    return fail(config, "newFiles.allow must be an array of strings");
  }
  const size_t count = yyjson_arr_size(allow);
  if (!allocate_patterns(count, config)) {
    return false;
  }

  size_t index;
  size_t maximum;
  yyjson_val *value;
  yyjson_arr_foreach(allow, index, maximum, value) {
    if (!copy_pattern(value, index, config)) {
      return false;
    }
  }
  return true;
}

static bool read_new_files(yyjson_val *root, cli_config *config) {
  yyjson_val *new_files = yyjson_obj_get(root, "newFiles");
  if (!yyjson_is_obj(new_files)) {
    return fail(config, "newFiles must be an object");
  }
  if (!validate_new_file_keys(new_files, config)) {
    return false;
  }
  return read_default(new_files, config) && read_allow(new_files, config);
}

static bool parse_document(yyjson_doc *document, cli_config *config) {
  yyjson_val *root = yyjson_doc_get_root(document);
  if (!yyjson_is_obj(root)) {
    return fail(config, "configuration must be an object");
  }
  if (!validate_root_keys(root, config)) {
    return false;
  }
  return read_version(root, config) && read_new_files(root, config);
}

static bool handle_missing_path(const char *root, cli_config *config) {
  if (config->error[0] == '\0') {
    return fail(config, "could not allocate configuration path");
  }
  config->source_path = copy_string(root);
  return false;
}

static char *locate_config(const char *root, const char *config_path,
                           cli_config *config) {
  if (config_path != NULL) {
    return resolve_config_path(root, config_path);
  }
  return legibility_discover_config(root, config->error, sizeof(config->error));
}

bool cli_config_load(const char *root, const char *config_path, cli_config *config) {
  memset(config, 0, sizeof(*config));
  config->source_path = locate_config(root, config_path, config);
  if (config->source_path == NULL) {
    return handle_missing_path(root, config);
  }

  const char *unsupported_format = format_error(config->source_path);
  if (unsupported_format != NULL) {
    return fail(config, unsupported_format);
  }

  yyjson_read_err error;
  yyjson_doc *document = yyjson_read_file(config->source_path, 0, NULL, &error);
  if (document == NULL) {
    return fail(config, error.msg);
  }
  const bool valid = parse_document(document, config);
  yyjson_doc_free(document);
  return valid;
}

void cli_config_destroy(cli_config *config) {
  for (size_t index = 0; index < config->policy.allow_pattern_count; index += 1) {
    free(config->owned_allow_patterns[index]);
  }
  free(config->owned_allow_patterns);
  free(config->source_path);
  memset(config, 0, sizeof(*config));
}
