#include "glob.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GLOB_MAX_EXPANDED_PATTERNS 4096

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
  bool negated;
} glob_pattern;

typedef struct {
  char *text;
  bool negated;
} expanded_pattern;

typedef struct {
  expanded_pattern *items;
  size_t count;
  size_t capacity;
} expanded_patterns;

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

static char *copy_pattern_text(const char *pattern) {
  const size_t size = strlen(pattern) + 1;
  char *copy = malloc(size);
  if (copy != NULL) {
    memcpy(copy, pattern, size);
  }
  return copy;
}

static bool grow_expanded_patterns(expanded_patterns *patterns) {
  const size_t doubled = patterns->capacity * 2;
  size_t capacity = patterns->capacity == 0 ? 8 : doubled;
  capacity =
      capacity > GLOB_MAX_EXPANDED_PATTERNS ? GLOB_MAX_EXPANDED_PATTERNS : capacity;
  expanded_pattern *items = realloc(patterns->items, capacity * sizeof(*items));
  if (items == NULL) {
    return false;
  }
  patterns->items = items;
  patterns->capacity = capacity;
  return true;
}

static bool append_expanded_pattern(expanded_patterns *patterns, const char *text,
                                    bool negated) {
  if (patterns->count == GLOB_MAX_EXPANDED_PATTERNS) {
    return false;
  }
  const bool full = patterns->count == patterns->capacity;
  if (full && !grow_expanded_patterns(patterns)) {
    return false;
  }
  char *copy = copy_pattern_text(text);
  if (copy == NULL) {
    return false;
  }
  patterns->items[patterns->count] = (expanded_pattern){
      .text = copy,
      .negated = negated,
  };
  patterns->count += 1;
  return true;
}

static void free_expanded_patterns(expanded_patterns *patterns) {
  for (size_t index = 0; index < patterns->count; index += 1) {
    free(patterns->items[index].text);
  }
  free(patterns->items);
}

static bool find_brace_group(const char *pattern, size_t *open, size_t *close) {
  size_t depth = 0;
  size_t group_open = 0;
  bool comma = false;
  for (size_t index = 0; pattern[index] != '\0'; index += 1) {
    const bool starts_group = pattern[index] == '{';
    const bool ends_group = pattern[index] == '}' && depth > 0;
    if (starts_group && depth++ == 0) {
      group_open = index;
    } else if (pattern[index] == ',' && depth == 1) {
      comma = true;
    } else if (ends_group && --depth == 0 && comma) {
      *open = group_open;
      *close = index;
      return true;
    }
  }
  return false;
}

static char *join_alternative(const char *pattern, size_t open, size_t close,
                              size_t start, size_t end) {
  const size_t prefix_length = open;
  const size_t alternative_length = end - start;
  const char *suffix = pattern + close + 1;
  const size_t suffix_length = strlen(suffix);
  const size_t size = prefix_length + alternative_length + suffix_length + 1;
  char *joined = malloc(size);
  if (joined == NULL) {
    return NULL;
  }
  memcpy(joined, pattern, prefix_length);
  memcpy(joined + prefix_length, pattern + start, alternative_length);
  memcpy(joined + prefix_length + alternative_length, suffix, suffix_length + 1);
  return joined;
}

static bool expand_pattern(expanded_patterns *patterns, const char *pattern,
                           bool negated);

static bool expand_alternative(expanded_patterns *patterns, const char *pattern,
                               bool negated, size_t open, size_t close, size_t start,
                               size_t end) {
  char *joined = join_alternative(pattern, open, close, start, end);
  const bool expanded = joined != NULL && expand_pattern(patterns, joined, negated);
  free(joined);
  return expanded;
}

static bool expand_brace_group(expanded_patterns *patterns, const char *pattern,
                               bool negated, size_t open, size_t close) {
  size_t depth = 0;
  size_t start = open + 1;
  for (size_t index = start; index <= close; index += 1) {
    depth += pattern[index] == '{' ? 1 : 0;
    depth -= pattern[index] == '}' && depth > 0 ? 1 : 0;
    const bool separator = pattern[index] == ',' && depth == 0;
    const bool last = index == close;
    if ((separator || last) &&
        !expand_alternative(patterns, pattern, negated, open, close, start, index)) {
      return false;
    }
    start = separator ? index + 1 : start;
  }
  return true;
}

static bool expand_pattern(expanded_patterns *patterns, const char *pattern,
                           bool negated) {
  size_t open;
  size_t close;
  if (!find_brace_group(pattern, &open, &close)) {
    return append_expanded_pattern(patterns, pattern, negated);
  }
  return expand_brace_group(patterns, pattern, negated, open, close);
}

static bool expand_input_patterns(const char *const *inputs, size_t count,
                                  expanded_patterns *patterns) {
  memset(patterns, 0, sizeof(*patterns));
  for (size_t index = 0; index < count; index += 1) {
    const bool negated = inputs[index][0] == '!';
    const char *pattern = negated ? inputs[index] + 1 : inputs[index];
    if (!expand_pattern(patterns, pattern, negated)) {
      free_expanded_patterns(patterns);
      return false;
    }
  }
  return true;
}

static bool count_token_capacity(const expanded_patterns *patterns, size_t *capacity) {
  *capacity = 0;
  for (size_t index = 0; index < patterns->count; index += 1) {
    const size_t length = strlen(patterns->items[index].text);
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
  const bool directory = starstar && is_separator(pattern[*index + 2]);
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
                             const expanded_patterns *patterns) {
  size_t token_offset = 0;
  for (size_t index = 0; index < matcher->pattern_count; index += 1) {
    glob_token *destination =
        matcher->tokens == NULL ? NULL : matcher->tokens + token_offset;
    const expanded_pattern source = patterns->items[index];
    const size_t count = compile_pattern(source.text, destination);
    matcher->patterns[index] = (glob_pattern){
        .offset = token_offset,
        .count = count,
        .negated = source.negated,
    };
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
  expanded_patterns expanded;
  if (!expand_input_patterns(patterns, pattern_count, &expanded)) {
    return NULL;
  }
  size_t token_capacity;
  if (!count_token_capacity(&expanded, &token_capacity)) {
    free_expanded_patterns(&expanded);
    return NULL;
  }
  legibility_glob_matcher *matcher =
      allocate_matcher(expanded.count, token_capacity, max_path_length);
  if (matcher == NULL) {
    free_expanded_patterns(&expanded);
    return NULL;
  }
  compile_patterns(matcher, &expanded);
  free_expanded_patterns(&expanded);
  return matcher;
}

bool legibility_glob_matcher_allows(legibility_glob_matcher *matcher, const char *path,
                                    bool default_allowed) {
  const size_t path_length = strlen(path);
  bool allowed = default_allowed;
  for (size_t index = 0; index < matcher->pattern_count; index += 1) {
    if (matches_pattern(matcher, matcher->patterns[index], path, path_length)) {
      allowed = !matcher->patterns[index].negated;
    }
  }
  return allowed;
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
