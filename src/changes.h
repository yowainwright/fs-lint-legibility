#ifndef LEGIBILITY_CHANGES_H
#define LEGIBILITY_CHANGES_H

#include "legibility.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
  legibility_change *items;
  size_t count;
  size_t capacity;
  char error[256];
} cli_changes;

bool cli_changes_read_nul(FILE *stream, cli_changes *changes);

bool cli_changes_read_git_staged(const char *root, cli_changes *changes);

bool cli_changes_read_git_base(const char *root, const char *base,
                               cli_changes *changes);

void cli_changes_destroy(cli_changes *changes);

#endif
