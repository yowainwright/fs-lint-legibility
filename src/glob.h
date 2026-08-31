#ifndef LEGIBILITY_GLOB_H
#define LEGIBILITY_GLOB_H

#include <stdbool.h>
#include <stddef.h>

typedef struct legibility_glob_matcher legibility_glob_matcher;

legibility_glob_matcher *legibility_glob_matcher_create(const char *const *patterns,
                                                        size_t pattern_count,
                                                        size_t max_path_length);

bool legibility_glob_matcher_allows(legibility_glob_matcher *matcher, const char *path,
                                    bool default_allowed);

void legibility_glob_matcher_destroy(legibility_glob_matcher *matcher);

#endif
