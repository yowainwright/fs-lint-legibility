#ifndef LEGIBILITY_GLOB_H
#define LEGIBILITY_GLOB_H

#include <stdbool.h>
#include <stddef.h>

typedef struct legibility_glob_matcher legibility_glob_matcher;

legibility_glob_matcher *legibility_glob_matcher_create(const char *const *patterns,
                                                        size_t pattern_count,
                                                        size_t max_path_length);

bool legibility_glob_matcher_matches(legibility_glob_matcher *matcher,
                                     const char *path);

void legibility_glob_matcher_destroy(legibility_glob_matcher *matcher);

#endif
