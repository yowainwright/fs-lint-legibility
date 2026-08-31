#include "glob.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GLOB_MATCH_UNKNOWN 0
#define GLOB_MATCH_FALSE 1
#define GLOB_MATCH_TRUE 2

typedef struct {
  char *text;
  bool negated;
} glob_pattern;

struct legibility_glob_matcher {
  glob_pattern *patterns;
  unsigned char *states;
  size_t pattern_count;
  size_t pattern_width;
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
                                     size_t pattern_count) {
  matcher->patterns = allocate_items(pattern_count, sizeof(*matcher->patterns));
  return pattern_count == 0 || matcher->patterns != NULL;
}

static bool copy_input_patterns(legibility_glob_matcher *matcher,
                                const char *const *patterns, size_t pattern_count) {
  for (size_t index = 0; index < pattern_count; index += 1) {
    const bool negated = patterns[index][0] == '!';
    const char *text = negated ? patterns[index] + 1 : patterns[index];
    matcher->patterns[index] = (glob_pattern){
        .text = copy_pattern_text(text),
        .negated = negated,
    };
    if (matcher->patterns[index].text == NULL) {
      return false;
    }
  }
  return true;
}

static bool find_pattern_width(const char *const *patterns, size_t pattern_count,
                               size_t *width) {
  *width = 1;
  for (size_t index = 0; index < pattern_count; index += 1) {
    const bool negated = patterns[index][0] == '!';
    const char *text = negated ? patterns[index] + 1 : patterns[index];
    const size_t length = strlen(text);
    if (length == SIZE_MAX) {
      return false;
    }
    const size_t candidate = length + 1;
    *width = candidate > *width ? candidate : *width;
  }
  return true;
}

static bool allocate_states(legibility_glob_matcher *matcher, size_t max_path_length) {
  if (max_path_length == SIZE_MAX) {
    return false;
  }
  matcher->row_width = max_path_length + 1;
  if (matcher->pattern_width > SIZE_MAX / matcher->row_width) {
    return false;
  }
  matcher->states = malloc(matcher->pattern_width * matcher->row_width);
  return matcher->states != NULL;
}

typedef struct {
  legibility_glob_matcher *matcher;
  const char *pattern;
  const char *path;
  size_t pattern_length;
  size_t path_length;
} glob_match_context;

static bool matches_from(glob_match_context *context, size_t pattern_index,
                         size_t path_index);

static unsigned char *match_state(glob_match_context *context, size_t pattern_index,
                                  size_t path_index) {
  const size_t offset = pattern_index * context->matcher->row_width + path_index;
  return context->matcher->states + offset;
}

static bool find_brace_group_at(const char *pattern, size_t open, size_t *close) {
  size_t depth = 0;
  bool comma = false;
  for (size_t index = open; pattern[index] != '\0'; index += 1) {
    const bool starts_group = pattern[index] == '{';
    const bool ends_group = pattern[index] == '}' && depth > 0;
    if (starts_group) {
      depth += 1;
    } else if (pattern[index] == ',' && depth == 1) {
      comma = true;
    } else if (ends_group && --depth == 0 && comma) {
      *close = index;
      return true;
    }
  }
  return false;
}

static bool matches_span(glob_match_context *context, size_t pattern_index, size_t end,
                         size_t after, size_t path_index);

static bool matches_continuation(glob_match_context *context, size_t pattern_index,
                                 size_t end, size_t after, size_t path_index) {
  const bool top_level =
      end == context->pattern_length && after == context->pattern_length;
  if (top_level) {
    return matches_from(context, pattern_index, path_index);
  }
  return matches_span(context, pattern_index, end, after, path_index);
}

static bool matches_span_star(glob_match_context *context, size_t pattern_index,
                              size_t end, size_t after, size_t path_index) {
  const bool zero =
      matches_continuation(context, pattern_index + 1, end, after, path_index);
  const bool can_consume =
      path_index < context->path_length && !is_separator(context->path[path_index]);
  return zero || (can_consume && matches_continuation(context, pattern_index, end,
                                                      after, path_index + 1));
}

static bool matches_span_starstar(glob_match_context *context, size_t pattern_index,
                                  size_t end, size_t after, size_t path_index) {
  const bool zero =
      matches_continuation(context, pattern_index + 2, end, after, path_index);
  const bool can_consume = path_index < context->path_length;
  return zero || (can_consume && matches_continuation(context, pattern_index, end,
                                                      after, path_index + 1));
}

static bool matches_span_directory(glob_match_context *context, size_t pattern_index,
                                   size_t end, size_t after, size_t path_index) {
  if (matches_continuation(context, pattern_index + 3, end, after, path_index)) {
    return true;
  }
  for (size_t index = path_index; index < context->path_length; index += 1) {
    if (is_separator(context->path[index]) &&
        matches_continuation(context, pattern_index, end, after, index + 1)) {
      return true;
    }
  }
  return false;
}

static bool matches_span_brace(glob_match_context *context, size_t pattern_index,
                               size_t close, size_t path_index) {
  size_t depth = 0;
  size_t start = pattern_index + 1;
  for (size_t index = start; index <= close; index += 1) {
    depth += context->pattern[index] == '{' ? 1 : 0;
    depth -= context->pattern[index] == '}' && depth > 0 ? 1 : 0;
    const bool separator = context->pattern[index] == ',' && depth == 0;
    const bool last = index == close;
    const bool matched = (separator || last) &&
                         matches_span(context, start, index, close + 1, path_index);
    if (matched) {
      return true;
    }
    start = separator ? index + 1 : start;
  }
  return false;
}

static bool matches_span(glob_match_context *context, size_t pattern_index, size_t end,
                         size_t after, size_t path_index) {
  if (pattern_index == end) {
    return matches_from(context, after, path_index);
  }
  const char token = context->pattern[pattern_index];
  size_t close;
  if (token == '{' && find_brace_group_at(context->pattern, pattern_index, &close) &&
      close <= end) {
    return matches_span_brace(context, pattern_index, close, path_index);
  }
  if (token == '*') {
    const bool starstar = context->pattern[pattern_index + 1] == '*';
    const bool directory =
        starstar && is_separator(context->pattern[pattern_index + 2]);
    if (directory) {
      return matches_span_directory(context, pattern_index, end, after, path_index);
    }
    if (starstar) {
      return matches_span_starstar(context, pattern_index, end, after, path_index);
    }
    return matches_span_star(context, pattern_index, end, after, path_index);
  }
  const bool can_consume = path_index < context->path_length;
  const bool question_match =
      token == '?' && can_consume && !is_separator(context->path[path_index]);
  const bool literal_match =
      can_consume && token != '?' && characters_match(token, context->path[path_index]);
  return (question_match || literal_match) &&
         matches_continuation(context, pattern_index + 1, end, after, path_index + 1);
}

static bool matches_from_uncached(glob_match_context *context, size_t pattern_index,
                                  size_t path_index) {
  if (pattern_index == context->pattern_length) {
    return path_index == context->path_length;
  }
  return matches_span(context, pattern_index, context->pattern_length,
                      context->pattern_length, path_index);
}

static bool matches_from(glob_match_context *context, size_t pattern_index,
                         size_t path_index) {
  unsigned char *state = match_state(context, pattern_index, path_index);
  if (*state != GLOB_MATCH_UNKNOWN) {
    return *state == GLOB_MATCH_TRUE;
  }
  const bool matched = matches_from_uncached(context, pattern_index, path_index);
  *state = matched ? GLOB_MATCH_TRUE : GLOB_MATCH_FALSE;
  return matched;
}

static bool matches_pattern(legibility_glob_matcher *matcher, glob_pattern pattern,
                            const char *path, size_t path_length) {
  glob_match_context context = {
      .matcher = matcher,
      .pattern = pattern.text,
      .path = path,
      .pattern_length = strlen(pattern.text),
      .path_length = path_length,
  };
  memset(matcher->states, GLOB_MATCH_UNKNOWN,
         (context.pattern_length + 1) * matcher->row_width);
  return matches_from(&context, 0, 0);
}

static legibility_glob_matcher *
allocate_matcher(size_t pattern_count, size_t pattern_width, size_t max_path_length) {
  legibility_glob_matcher *matcher = calloc(1, sizeof(*matcher));
  if (matcher == NULL) {
    return NULL;
  }
  matcher->pattern_count = pattern_count;
  matcher->pattern_width = pattern_width;
  const bool storage_ready = allocate_pattern_storage(matcher, pattern_count);
  const bool states_ready = storage_ready && allocate_states(matcher, max_path_length);
  if (!states_ready) {
    legibility_glob_matcher_destroy(matcher);
    return NULL;
  }
  return matcher;
}

legibility_glob_matcher *legibility_glob_matcher_create(const char *const *patterns,
                                                        size_t pattern_count,
                                                        size_t max_path_length) {
  size_t pattern_width;
  if (!find_pattern_width(patterns, pattern_count, &pattern_width)) {
    return NULL;
  }
  legibility_glob_matcher *matcher =
      allocate_matcher(pattern_count, pattern_width, max_path_length);
  if (matcher == NULL) {
    return NULL;
  }
  if (!copy_input_patterns(matcher, patterns, pattern_count)) {
    legibility_glob_matcher_destroy(matcher);
    return NULL;
  }
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
  free(matcher->states);
  for (size_t index = 0; index < matcher->pattern_count; index += 1) {
    free(matcher->patterns[index].text);
  }
  free(matcher->patterns);
  free(matcher);
}
