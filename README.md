# fs-lint

`fs-lint` pushes back on unnecessary files and bespoke filenames. It provides a
dependency-free C17 policy library and a small command-line adapter.

This is an early preview. The first policy denies new files by default while
allowing established path patterns.

## Build

<!-- build and test commands matching CMakeLists.txt -->

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --build build --target e2e
```

## Configuration

<!-- configuration filenames and schema supported by src/discover.c and src/config.c -->

Strict JSON is supported in `.legibilityrc` and `.legibilityrc.json`:

```json
{
  "version": 1,
  "newFiles": {
    "default": "deny",
    "allow": ["README.md", "src/**/index.c", "**/*.test.c"]
  }
}
```

`newFiles.default` defaults to `"deny"` when omitted.

Configuration discovery starts at the lint root and stops at the repository
root. Multiple configuration files in one directory are an error. YAML and
TOML readers are reserved for later adapters.

## CLI

<!-- command syntax and exit codes implemented by src/main.c -->

```sh
fs-lint check-path src/new-helper.c
fs-lint check-path --format json src/new-helper.c
fs-lint check-path --root . --config .legibilityrc.json src/new-helper.c
fs-lint check --staged
fs-lint check --base origin/main
git diff --name-only --diff-filter=A --no-renames -z | fs-lint check --stdin0
```

`check` accepts exactly one change source. `--stdin0` treats each NUL-delimited
path as added, `--staged` checks added paths in the Git index, and `--base`
checks additions on `HEAD` since its merge base with a Git ref. Rename
destinations are checked as additions.

Exit code `0` allows the path, `1` reports policy violations, and `2` reports
configuration or usage errors.

## Library

<!-- public types and functions exported by include/legibility.h -->

`liblegibility` accepts normalized configuration and file changes through one
function. Paths and allow patterns are limited to 4,096 characters.

```c
const legibility_config config = {
  .new_files_default = LEGIBILITY_NEW_FILES_DENY,
};
const legibility_change change = {
  .path = "src/new-helper.c",
  .kind = LEGIBILITY_CHANGE_ADDED,
};

legibility_status status = legibility_check(
  &config,
  &change,
  1,
  report_diagnostic,
  context
);
```

Configuration parsing, Git integration, and agent hooks remain outside the
core library.
