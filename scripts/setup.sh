#!/bin/sh
set -eu

run_suite() {
  name="${1:?}"
  build_type="${2:?}"
  build_dir="$root/build-hooks-$name"
  prepare_build_dir "$build_dir"
  printf 'setup: %s build\n' "$name"
  cmake -S "$root" -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type" -DCMAKE_C_FLAGS=-Werror
  cmake --build "$build_dir" --parallel
  printf 'setup: %s tests\n' "$name"
  ctest --test-dir "$build_dir" --output-on-failure
}

prepare_build_dir() {
  build_dir="${1:?}"
  cache="$build_dir/CMakeCache.txt"
  [ -f "$cache" ] || return 0
  grep -Fxq "CMAKE_HOME_DIRECTORY:INTERNAL=$root" "$cache" || rm -rf "$build_dir"
}

run_sanitizers() {
  sanitizer_dir="$root/build-hooks-sanitized"
  prepare_build_dir "$sanitizer_dir"
  compile_flags="-Werror -fsanitize=address,undefined -fno-omit-frame-pointer"
  linker_flags="-fsanitize=address,undefined"
  printf 'setup: sanitizer build\n'
  cmake -S "$root" -B "$sanitizer_dir" -DCMAKE_BUILD_TYPE=Debug \
    "-DCMAKE_C_FLAGS=$compile_flags" "-DCMAKE_EXE_LINKER_FLAGS=$linker_flags"
  cmake --build "$sanitizer_dir" --parallel
  asan_options="strict_string_checks=1"
  [ "$(uname -s)" != "Linux" ] || asan_options="detect_leaks=1:$asan_options"
  printf 'setup: sanitizer tests\n'
  ASAN_OPTIONS="$asan_options" UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    ctest --test-dir "$sanitizer_dir" --output-on-failure --exclude-regex '^e2e-install$'
}

skip_hooks() {
  [ "${FS_LINT_SKIP_HOOKS:-0}" = "1" ]
}

run_pre_commit() {
  skip_hooks && return 0
  cd "$root"
  printf 'setup: staged diff check\n'
  git --no-pager diff --cached --check
  run_shell_checks
  printf 'setup: C format check\n'
  clang-format --dry-run --Werror include/*.h src/*.c src/*.h tests/*.c
  run_suite debug Debug
}

run_shell_checks() {
  printf 'setup: shell format check\n'
  shfmt -d -i 2 scripts/setup.sh tests/setup_test.sh
  printf 'setup: shellcheck\n'
  shellcheck scripts/setup.sh tests/setup_test.sh
  command -v shellcheck-legibility >/dev/null 2>&1 || return 0
  printf 'setup: shellcheck-legibility\n'
  shellcheck-legibility check scripts/setup.sh tests/setup_test.sh
}

run_pre_push() {
  skip_hooks && return 0
  run_suite release Release
  run_sanitizers
}

resolve_hooks_dir() {
  hooks_dir="$(git -C "$root" rev-parse --git-path hooks)"
  case "$hooks_dir" in
  /*) ;;
  *) hooks_dir="$root/$hooks_dir" ;;
  esac
}

is_unmanaged_hook() {
  target="${1:?}"
  [ -L "$target" ] && return 0
  [ -e "$target" ] || return 1
  grep -Fxq "$marker" "$target" && return 1
  return 0
}

check_hook_target() {
  target="${1:?}"
  is_unmanaged_hook "$target" || return 0
  printf 'setup: refusing to overwrite unmanaged hook: %s\n' "$target" >&2
  exit 1
}

check_hook_targets() {
  for name in pre-commit pre-push; do
    check_hook_target "$hooks_dir/$name"
  done
}

remove_obsolete_hook() {
  obsolete="$hooks_dir/post-merge"
  [ -f "$obsolete" ] || return 0
  [ -L "$obsolete" ] && return 0
  grep -Fxq "$marker" "$obsolete" || return 0
  rm "$obsolete"
}

write_hook() {
  name="${1:?}"
  target="$hooks_dir/$name"
  temp="$(mktemp "$target.tmp.XXXXXX")"
  cat >"$temp" <<HOOK
#!/bin/sh
$marker
set -eu
exec "\$(git rev-parse --show-toplevel)/scripts/setup.sh" "$name"
HOOK
  hook_is_current "$target" "$temp" && remove_hook_temp "$temp" && return 0
  install_hook "$target" "$temp"
}

remove_hook_temp() {
  temp="${1:?}"
  rm "$temp"
}

install_hook() {
  target="${1:?}"
  temp="${2:?}"
  chmod 755 "$temp"
  mv "$temp" "$target"
  printf 'setup: installed %s\n' "$target"
}

hook_is_current() {
  target="${1:?}"
  temp="${2:?}"
  [ -x "$target" ] || return 1
  cmp -s "$temp" "$target"
}

write_hooks() {
  for name in pre-commit pre-push; do
    write_hook "$name"
  done
}

run_install() {
  configured="$(git -C "$root" config --get core.hooksPath || true)"
  [ -z "$configured" ] || fail_configured_hooks_path "$configured"

  resolve_hooks_dir
  mkdir -p "$hooks_dir"
  check_hook_targets
  remove_obsolete_hook
  write_hooks
}

fail_configured_hooks_path() {
  configured="${1:?}"
  printf 'setup: core.hooksPath is already set to %s\n' "$configured" >&2
  exit 1
}

dispatch() {
  mode="${1:?}"
  case "$mode" in
  pre-commit) run_pre_commit ;;
  pre-push) run_pre_push ;;
  install) run_install ;;
  *)
    printf 'usage: scripts/setup.sh\n' >&2
    exit 1
    ;;
  esac
}

main() {
  marker="# fs-lint-legibility managed hook"
  script_dir="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
  root="$(git -C "$script_dir/.." rev-parse --show-toplevel)"
  mode="${1:-install}"
  [ "$#" -le 1 ] || mode="invalid"
  dispatch "$mode"
}

main "$@"
