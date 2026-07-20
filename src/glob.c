#include "glob.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *pattern;
  const char *path;
  size_t pattern_length;
  size_t path_length;
  signed char *memo;
} glob_state;

static bool match_at(glob_state *state, size_t pattern_index, size_t path_index);

static bool is_separator(char value) { return value == '/' || value == '\\'; }

static bool characters_match(char pattern, char path) {
  const bool both_separators = is_separator(pattern) && is_separator(path);
  return both_separators || pattern == path;
}

static bool match_globstar_directory(glob_state *state, size_t pattern_index,
                                     size_t path_index) {
  if (match_at(state, pattern_index + 3, path_index)) {
    return true;
  }
  size_t next_separator = path_index;
  while (next_separator < state->path_length &&
         !is_separator(state->path[next_separator])) {
    next_separator += 1;
  }
  if (next_separator == state->path_length) {
    return false;
  }
  return match_at(state, pattern_index, next_separator + 1);
}

static bool match_globstar(glob_state *state, size_t pattern_index, size_t path_index) {
  if (match_at(state, pattern_index + 2, path_index)) {
    return true;
  }
  const bool has_path_character = path_index < state->path_length;
  return has_path_character && match_at(state, pattern_index, path_index + 1);
}

static bool match_star(glob_state *state, size_t pattern_index, size_t path_index) {
  if (match_at(state, pattern_index + 1, path_index)) {
    return true;
  }
  const bool has_path_character = path_index < state->path_length;
  const bool within_segment =
      has_path_character && !is_separator(state->path[path_index]);
  return within_segment && match_at(state, pattern_index, path_index + 1);
}

static bool match_unmemoized(glob_state *state, size_t pattern_index,
                             size_t path_index) {
  if (pattern_index == state->pattern_length) {
    return path_index == state->path_length;
  }

  const char token = state->pattern[pattern_index];
  const bool is_globstar = token == '*' && state->pattern[pattern_index + 1] == '*';
  const bool is_directory_globstar =
      is_globstar && state->pattern[pattern_index + 2] == '/';
  if (is_directory_globstar) {
    return match_globstar_directory(state, pattern_index, path_index);
  }
  if (is_globstar) {
    return match_globstar(state, pattern_index, path_index);
  }
  if (token == '*') {
    return match_star(state, pattern_index, path_index);
  }
  if (path_index == state->path_length) {
    return false;
  }
  if (token == '?') {
    const bool within_segment = !is_separator(state->path[path_index]);
    return within_segment && match_at(state, pattern_index + 1, path_index + 1);
  }
  const bool equal = characters_match(token, state->path[path_index]);
  return equal && match_at(state, pattern_index + 1, path_index + 1);
}

static bool match_at(glob_state *state, size_t pattern_index, size_t path_index) {
  const size_t width = state->path_length + 1;
  signed char *memo = &state->memo[(pattern_index * width) + path_index];
  if (*memo >= 0) {
    return *memo == 1;
  }
  const bool matched = match_unmemoized(state, pattern_index, path_index);
  *memo = matched ? 1 : 0;
  return matched;
}

static signed char *create_memo(size_t pattern_length, size_t path_length) {
  const size_t rows = pattern_length + 1;
  const size_t columns = path_length + 1;
  if (columns != 0 && rows > SIZE_MAX / columns) {
    return NULL;
  }
  const size_t size = rows * columns;
  signed char *memo = malloc(size);
  if (memo == NULL) {
    return NULL;
  }
  for (size_t index = 0; index < size; index += 1) {
    memo[index] = -1;
  }
  return memo;
}

bool legibility_glob_matches(const char *pattern, const char *path) {
  glob_state state = {
      .pattern = pattern,
      .path = path,
      .pattern_length = strlen(pattern),
      .path_length = strlen(path),
      .memo = NULL,
  };
  state.memo = create_memo(state.pattern_length, state.path_length);
  if (state.memo == NULL) {
    return false;
  }
  const bool matched = match_at(&state, 0, 0);
  free(state.memo);
  return matched;
}
