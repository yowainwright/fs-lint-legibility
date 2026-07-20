#include "glob.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  GLOB_LITERAL,
  GLOB_QUESTION,
  GLOB_STAR,
  GLOB_STARSTAR,
  GLOB_STARSTAR_DIRECTORY
} glob_token_kind;

typedef struct {
  glob_token_kind kind;
  char literal;
} glob_token;

typedef struct {
  size_t offset;
  size_t count;
} glob_pattern;

struct legibility_glob_matcher {
  glob_pattern *patterns;
  glob_token *tokens;
  bool *rows;
  size_t pattern_count;
  size_t row_width;
};

static bool is_separator(char value) { return value == '/' || value == '\\'; }

static bool characters_match(char pattern, char path) {
  const bool both_separators = is_separator(pattern) && is_separator(path);
  return both_separators || pattern == path;
}

static bool count_token_capacity(const char *const *patterns, size_t count,
                                 size_t *capacity) {
  *capacity = 0;
  for (size_t index = 0; index < count; index += 1) {
    const size_t length = strlen(patterns[index]);
    if (length > SIZE_MAX - *capacity) {
      return false;
    }
    *capacity += length;
  }
  return true;
}

static void *allocate_items(size_t count, size_t item_size) {
  if (count == 0) {
    return NULL;
  }
  if (count > SIZE_MAX / item_size) {
    return NULL;
  }
  return malloc(count * item_size);
}

static bool allocate_pattern_storage(legibility_glob_matcher *matcher,
                                     size_t pattern_count, size_t token_capacity) {
  matcher->patterns = allocate_items(pattern_count, sizeof(*matcher->patterns));
  if (pattern_count > 0 && matcher->patterns == NULL) {
    return false;
  }
  matcher->tokens = allocate_items(token_capacity, sizeof(*matcher->tokens));
  return token_capacity == 0 || matcher->tokens != NULL;
}

static bool allocate_rows(legibility_glob_matcher *matcher, size_t max_path_length) {
  const bool width_overflow = max_path_length == SIZE_MAX;
  if (width_overflow) {
    return false;
  }
  matcher->row_width = max_path_length + 1;
  const bool size_overflow = matcher->row_width > SIZE_MAX / (2 * sizeof(bool));
  if (size_overflow) {
    return false;
  }
  matcher->rows = calloc(matcher->row_width * 2, sizeof(bool));
  return matcher->rows != NULL;
}

static glob_token read_star_token(const char *pattern, size_t *index) {
  const bool starstar = pattern[*index + 1] == '*';
  const bool directory = starstar && pattern[*index + 2] == '/';
  if (directory) {
    *index += 3;
    return (glob_token){.kind = GLOB_STARSTAR_DIRECTORY};
  }
  if (starstar) {
    *index += 2;
    return (glob_token){.kind = GLOB_STARSTAR};
  }
  *index += 1;
  return (glob_token){.kind = GLOB_STAR};
}

static glob_token read_token(const char *pattern, size_t *index) {
  const char value = pattern[*index];
  if (value == '?') {
    *index += 1;
    return (glob_token){.kind = GLOB_QUESTION};
  }
  if (value == '*') {
    return read_star_token(pattern, index);
  }
  *index += 1;
  return (glob_token){.kind = GLOB_LITERAL, .literal = value};
}

static size_t compile_pattern(const char *pattern, glob_token *tokens) {
  size_t pattern_index = 0;
  size_t token_count = 0;
  while (pattern[pattern_index] != '\0') {
    tokens[token_count] = read_token(pattern, &pattern_index);
    token_count += 1;
  }
  return token_count;
}

static void compile_patterns(legibility_glob_matcher *matcher,
                             const char *const *patterns) {
  size_t token_offset = 0;
  for (size_t index = 0; index < matcher->pattern_count; index += 1) {
    glob_token *destination =
        matcher->tokens == NULL ? NULL : matcher->tokens + token_offset;
    const size_t count = compile_pattern(patterns[index], destination);
    matcher->patterns[index] = (glob_pattern){.offset = token_offset, .count = count};
    token_offset += count;
  }
}

static bool token_matches(glob_token token, char path) {
  if (token.kind == GLOB_QUESTION) {
    return !is_separator(path);
  }
  return characters_match(token.literal, path);
}

static void fill_character_row(glob_token token, const char *path, size_t path_length,
                               const bool *next, bool *current) {
  current[path_length] = false;
  for (size_t cursor = path_length; cursor > 0; cursor -= 1) {
    const size_t index = cursor - 1;
    current[index] = token_matches(token, path[index]) && next[index + 1];
  }
}

static void fill_star_row(const char *path, size_t path_length, const bool *next,
                          bool *current) {
  current[path_length] = next[path_length];
  for (size_t cursor = path_length; cursor > 0; cursor -= 1) {
    const size_t index = cursor - 1;
    const bool consumes = !is_separator(path[index]) && current[index + 1];
    current[index] = next[index] || consumes;
  }
}

static void fill_starstar_row(size_t path_length, const bool *next, bool *current) {
  current[path_length] = next[path_length];
  for (size_t cursor = path_length; cursor > 0; cursor -= 1) {
    const size_t index = cursor - 1;
    current[index] = next[index] || current[index + 1];
  }
}

static void fill_directory_row(const char *path, size_t path_length, const bool *next,
                               bool *current) {
  current[path_length] = next[path_length];
  size_t separator = path_length;
  for (size_t cursor = path_length; cursor > 0; cursor -= 1) {
    const size_t index = cursor - 1;
    separator = is_separator(path[index]) ? index : separator;
    const bool has_separator = separator < path_length;
    const bool consumes = has_separator && current[separator + 1];
    current[index] = next[index] || consumes;
  }
}

static void fill_row(glob_token token, const char *path, size_t path_length,
                     const bool *next, bool *current) {
  if (token.kind == GLOB_STAR) {
    fill_star_row(path, path_length, next, current);
    return;
  }
  if (token.kind == GLOB_STARSTAR) {
    fill_starstar_row(path_length, next, current);
    return;
  }
  if (token.kind == GLOB_STARSTAR_DIRECTORY) {
    fill_directory_row(path, path_length, next, current);
    return;
  }
  fill_character_row(token, path, path_length, next, current);
}

static bool matches_pattern(legibility_glob_matcher *matcher, glob_pattern pattern,
                            const char *path, size_t path_length) {
  bool *next = matcher->rows;
  bool *current = matcher->rows + matcher->row_width;
  memset(next, 0, matcher->row_width * sizeof(*next));
  next[path_length] = true;
  for (size_t cursor = pattern.count; cursor > 0; cursor -= 1) {
    const glob_token token = matcher->tokens[pattern.offset + cursor - 1];
    fill_row(token, path, path_length, next, current);
    bool *swap = next;
    next = current;
    current = swap;
  }
  return next[0];
}

static legibility_glob_matcher *
allocate_matcher(size_t pattern_count, size_t token_capacity, size_t max_path_length) {
  legibility_glob_matcher *matcher = calloc(1, sizeof(*matcher));
  if (matcher == NULL) {
    return NULL;
  }
  matcher->pattern_count = pattern_count;
  const bool storage_ready =
      allocate_pattern_storage(matcher, pattern_count, token_capacity);
  const bool rows_ready = storage_ready && allocate_rows(matcher, max_path_length);
  if (!rows_ready) {
    legibility_glob_matcher_destroy(matcher);
    return NULL;
  }
  return matcher;
}

legibility_glob_matcher *legibility_glob_matcher_create(const char *const *patterns,
                                                        size_t pattern_count,
                                                        size_t max_path_length) {
  size_t token_capacity;
  if (!count_token_capacity(patterns, pattern_count, &token_capacity)) {
    return NULL;
  }
  legibility_glob_matcher *matcher =
      allocate_matcher(pattern_count, token_capacity, max_path_length);
  if (matcher == NULL) {
    return NULL;
  }
  compile_patterns(matcher, patterns);
  return matcher;
}

bool legibility_glob_matcher_matches(legibility_glob_matcher *matcher,
                                     const char *path) {
  const size_t path_length = strlen(path);
  for (size_t index = 0; index < matcher->pattern_count; index += 1) {
    if (matches_pattern(matcher, matcher->patterns[index], path, path_length)) {
      return true;
    }
  }
  return false;
}

void legibility_glob_matcher_destroy(legibility_glob_matcher *matcher) {
  if (matcher == NULL) {
    return;
  }
  free(matcher->rows);
  free(matcher->tokens);
  free(matcher->patterns);
  free(matcher);
}
