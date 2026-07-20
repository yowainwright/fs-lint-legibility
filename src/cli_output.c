#include "cli_output.h"

static const char *safe_string(const char *value) { return value == NULL ? "" : value; }

static const char *severity_name(legibility_severity severity) {
  return severity == LEGIBILITY_SEVERITY_WARNING ? "warning" : "error";
}

static void write_control_character(FILE *stream, unsigned char value) {
  switch (value) {
  case '\b':
    fputs("\\b", stream);
    return;
  case '\f':
    fputs("\\f", stream);
    return;
  case '\n':
    fputs("\\n", stream);
    return;
  case '\r':
    fputs("\\r", stream);
    return;
  case '\t':
    fputs("\\t", stream);
    return;
  default:
    fprintf(stream, "\\u%04x", value);
  }
}

static void write_nonquote_character(FILE *stream, unsigned char value) {
  if (value < 0x20) {
    write_control_character(stream, value);
    return;
  }
  fputc(value, stream);
}

static void write_json_character(FILE *stream, unsigned char value) {
  const int needs_escape = value == '"' || value == '\\';
  if (needs_escape) {
    fputc('\\', stream);
    fputc(value, stream);
    return;
  }
  write_nonquote_character(stream, value);
}

static void write_json_string(FILE *stream, const char *value) {
  value = safe_string(value);
  fputc('"', stream);
  while (*value != '\0') {
    write_json_character(stream, (unsigned char)*value);
    value += 1;
  }
  fputc('"', stream);
}

static void report_json(const legibility_diagnostic *diagnostic, FILE *stream) {
  fputs("{\"severity\":", stream);
  write_json_string(stream, severity_name(diagnostic->severity));
  fputs(",\"code\":", stream);
  write_json_string(stream, diagnostic->code);
  fputs(",\"path\":", stream);
  write_json_string(stream, diagnostic->path);
  fputs(",\"message\":", stream);
  write_json_string(stream, diagnostic->message);
  fputs("}\n", stream);
}

static void report_text(const legibility_diagnostic *diagnostic, FILE *stream) {
  const char *path = safe_string(diagnostic->path);
  const char *code = safe_string(diagnostic->code);
  const char *message = safe_string(diagnostic->message);
  fprintf(stream, "%s: %s %s: %s\n", path, severity_name(diagnostic->severity), code,
          message);
}

void cli_report(const legibility_diagnostic *diagnostic, void *user_data) {
  cli_output *output = user_data;
  if (output->format == CLI_OUTPUT_JSON) {
    report_json(diagnostic, output->stream);
    return;
  }
  report_text(diagnostic, output->stream);
}
