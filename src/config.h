#ifndef LEGIBILITY_CONFIG_H
#define LEGIBILITY_CONFIG_H

#include "legibility.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  legibility_config policy;
  char **owned_allow_patterns;
  char *source_path;
  char error[256];
} cli_config;

bool cli_config_load(const char *root, const char *config_path, cli_config *config);

void cli_config_destroy(cli_config *config);

#endif
