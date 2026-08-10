#!/bin/sh
set -eu

marker="# fs-lint-legibility managed hook"
script_dir="$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
root="$(git -C "$script_dir/.." rev-parse --show-toplevel)"
mode="${1:-install}"
[ "$#" -le 1 ] || mode="invalid"

run_suite() {
  name="$1"
  build_type="$2"
  build_dir="$root/build-hooks-$name"
  cmake -S "$root" -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type" -DCMAKE_C_FLAGS=-Werror
  cmake --build "$build_dir" --parallel
  ctest --test-dir "$build_dir" --output-on-failure
}

run_sanitizers() {
  sanitizer_dir="$root/build-hooks-sanitized"
  compile_flags="-Werror -fsanitize=address,undefined -fno-omit-frame-pointer"
  linker_flags="-fsanitize=address,undefined"
  cmake -S "$root" -B "$sanitizer_dir" -DCMAKE_BUILD_TYPE=Debug \
    "-DCMAKE_C_FLAGS=$compile_flags" "-DCMAKE_EXE_LINKER_FLAGS=$linker_flags"
  cmake --build "$sanitizer_dir" --parallel
  asan_options="strict_string_checks=1"
  [ "$(uname -s)" != "Linux" ] || asan_options="detect_leaks=1:$asan_options"
  ASAN_OPTIONS="$asan_options" UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    ctest --test-dir "$sanitizer_dir" --output-on-failure --exclude-regex '^e2e-install$'
}

case "$mode" in
  pre-commit)
    [ "${FS_LINT_SKIP_HOOKS:-0}" != "1" ] || exit 0
    cd "$root"
    git diff --cached --check
    clang-format --dry-run --Werror include/*.h src/*.c src/*.h tests/*.c
    run_suite debug Debug
    ;;
  pre-push)
    [ "${FS_LINT_SKIP_HOOKS:-0}" != "1" ] || exit 0
    run_suite release Release
    run_sanitizers
    ;;
  install | post-merge)
    if [ "$mode" = "post-merge" ] && [ "${FS_LINT_SKIP_HOOKS:-0}" = "1" ]; then
      exit 0
    fi
    configured="$(git -C "$root" config --get core.hooksPath || true)"
    if [ -n "$configured" ]; then
      [ "$mode" != "post-merge" ] || exit 0
      printf 'setup: core.hooksPath is already set to %s\n' "$configured" >&2
      exit 1
    fi

    hooks_dir="$(git -C "$root" rev-parse --git-path hooks)"
    case "$hooks_dir" in
      /*) ;;
      *) hooks_dir="$root/$hooks_dir" ;;
    esac
    mkdir -p "$hooks_dir"

    for name in pre-commit pre-push post-merge; do
      target="$hooks_dir/$name"
      if [ -L "$target" ] || { [ -e "$target" ] && ! grep -Fxq "$marker" "$target"; }; then
        printf 'setup: refusing to overwrite unmanaged hook: %s\n' "$target" >&2
        exit 1
      fi
    done

    for name in pre-commit pre-push post-merge; do
      target="$hooks_dir/$name"
      temp="$(mktemp "$target.tmp.XXXXXX")"
      cat >"$temp" <<HOOK
#!/bin/sh
$marker
set -eu
exec "\$(git rev-parse --show-toplevel)/scripts/setup.sh" "$name"
HOOK
      if [ -f "$target" ] && cmp -s "$temp" "$target"; then
        rm "$temp"
        continue
      fi
      chmod 755 "$temp"
      mv "$temp" "$target"
      printf 'setup: installed %s\n' "$target"
    done
    ;;
  *)
    printf 'usage: scripts/setup.sh\n' >&2
    exit 1
    ;;
esac
