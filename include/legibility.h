#ifndef LEGIBILITY_H
#define LEGIBILITY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  LEGIBILITY_STATUS_OK = 0,
  LEGIBILITY_STATUS_VIOLATIONS = 1,
  LEGIBILITY_STATUS_ERROR = 2
} legibility_status;

typedef enum {
  LEGIBILITY_CHANGE_ADDED,
  LEGIBILITY_CHANGE_MODIFIED,
  LEGIBILITY_CHANGE_DELETED
} legibility_change_kind;

typedef enum {
  LEGIBILITY_SEVERITY_WARNING,
  LEGIBILITY_SEVERITY_ERROR
} legibility_severity;

typedef struct {
  const char *path;
  legibility_change_kind kind;
} legibility_change;

typedef enum {
  LEGIBILITY_NEW_FILES_ALLOW,
  LEGIBILITY_NEW_FILES_DENY
} legibility_new_files_default;

typedef struct {
  legibility_new_files_default new_files_default;
  const char *const *allow_patterns;
  size_t allow_pattern_count;
} legibility_config;

typedef struct {
  legibility_severity severity;
  const char *code;
  const char *path;
  const char *message;
} legibility_diagnostic;

typedef void (*legibility_reporter)(const legibility_diagnostic *diagnostic,
                                    void *user_data);

legibility_status legibility_check(const legibility_config *config,
                                   const legibility_change *changes,
                                   size_t change_count, legibility_reporter reporter,
                                   void *user_data);

#ifdef __cplusplus
}
#endif

#endif
