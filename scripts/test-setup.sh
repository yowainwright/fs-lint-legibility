#!/usr/bin/env bash

set -euo pipefail

FS_LINT_TEST_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly FS_LINT_TEST_SCRIPT_DIR
readonly FS_LINT_TEST_SETUP="$FS_LINT_TEST_SCRIPT_DIR/setup.sh"
readonly FS_LINT_TEST_SOURCE_DIR="$FS_LINT_TEST_SCRIPT_DIR/hooks"
readonly FS_LINT_TEST_MARKER="# fs-lint-legibility managed hook"
FS_LINT_TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/fs-lint-setup-test.XXXXXX")"
readonly FS_LINT_TEST_ROOT
FS_LINT_TEST_COUNT=0

# shellcheck disable=SC1090,SC1091
FS_LINT_SETUP_TESTING=1 source "$FS_LINT_TEST_SETUP"

cleanup() {
  [[ -n "$FS_LINT_TEST_ROOT" ]] || return
  rm -rf "$FS_LINT_TEST_ROOT"
}

trap cleanup EXIT INT TERM

pass() {
  FS_LINT_TEST_COUNT=$((FS_LINT_TEST_COUNT + 1))
  printf 'ok %s - %s\n' "$FS_LINT_TEST_COUNT" "$1"
}

fail_test() {
  printf 'not ok - %s\n' "$1" >&2
  exit 1
}

run_setup() (
  local fs_lint_hooks_dir="$1"
  assert_source_hooks
  install_hooks "$fs_lint_hooks_dir"
)

create_fixture_repo() {
  local fs_lint_repo_dir="$1"
  mkdir -p "$fs_lint_repo_dir/scripts"
  cp "$FS_LINT_TEST_SETUP" "$fs_lint_repo_dir/scripts/setup.sh"
  cp -R "$FS_LINT_TEST_SOURCE_DIR" "$fs_lint_repo_dir/scripts/hooks"
  git -C "$fs_lint_repo_dir" init -q
}

assert_hook_matches_source() {
  local fs_lint_hooks_dir="$1"
  local fs_lint_hook_name="$2"
  local fs_lint_target="$fs_lint_hooks_dir/$fs_lint_hook_name"
  [[ -x "$fs_lint_target" ]] || fail_test "$fs_lint_target is not executable"
  cmp -s "$FS_LINT_TEST_SOURCE_DIR/$fs_lint_hook_name" "$fs_lint_target" ||
    fail_test "$fs_lint_target does not match its source"
}

test_installs_all_hooks() {
  local fs_lint_hooks_dir="$FS_LINT_TEST_ROOT/install"
  local fs_lint_hook_name
  run_setup "$fs_lint_hooks_dir" >/dev/null
  for fs_lint_hook_name in pre-commit pre-push post-merge; do
    assert_hook_matches_source "$fs_lint_hooks_dir" "$fs_lint_hook_name"
  done
  pass "installs executable managed hooks"
}

test_rerun_is_idempotent() {
  local fs_lint_hooks_dir="$FS_LINT_TEST_ROOT/idempotent"
  local fs_lint_output
  run_setup "$fs_lint_hooks_dir" >/dev/null
  fs_lint_output="$(run_setup "$fs_lint_hooks_dir")"
  [[ "$fs_lint_output" == *"unchanged"* ]] || fail_test "rerun did not report unchanged hooks"
  assert_hook_matches_source "$fs_lint_hooks_dir" pre-commit
  pass "rerun leaves current hooks unchanged"
}

test_updates_stale_managed_hook() {
  local fs_lint_hooks_dir="$FS_LINT_TEST_ROOT/update"
  run_setup "$fs_lint_hooks_dir" >/dev/null
  printf '#!/usr/bin/env bash\n%s\nexit 1\n' "$FS_LINT_TEST_MARKER" \
    >"$fs_lint_hooks_dir/pre-commit"
  run_setup "$fs_lint_hooks_dir" >/dev/null
  assert_hook_matches_source "$fs_lint_hooks_dir" pre-commit
  pass "updates stale managed hooks"
}

test_refuses_unmanaged_hook_atomically() {
  local fs_lint_hooks_dir="$FS_LINT_TEST_ROOT/unmanaged"
  mkdir -p "$fs_lint_hooks_dir"
  printf '#!/usr/bin/env bash\nexit 0\n' >"$fs_lint_hooks_dir/pre-push"
  if run_setup "$fs_lint_hooks_dir" >/dev/null 2>&1; then
    fail_test "setup accepted an unmanaged hook"
  fi
  [[ ! -e "$fs_lint_hooks_dir/pre-commit" ]] || fail_test "setup partially installed hooks"
  grep -Fq "exit 0" "$fs_lint_hooks_dir/pre-push" || fail_test "setup changed unmanaged hook"
  pass "refuses unmanaged hooks without partial installation"
}

test_refuses_symlink_hook() {
  local fs_lint_hooks_dir="$FS_LINT_TEST_ROOT/symlink"
  local fs_lint_external_hook="$FS_LINT_TEST_ROOT/external-hook"
  mkdir -p "$fs_lint_hooks_dir"
  printf '%s\n' "$FS_LINT_TEST_MARKER" >"$fs_lint_external_hook"
  ln -s "$fs_lint_external_hook" "$fs_lint_hooks_dir/pre-commit"
  if run_setup "$fs_lint_hooks_dir" >/dev/null 2>&1; then
    fail_test "setup accepted a symlink hook"
  fi
  [[ -L "$fs_lint_hooks_dir/pre-commit" ]] || fail_test "setup replaced symlink hook"
  pass "refuses symlink hooks"
}

test_rejects_arguments() {
  local fs_lint_repo_dir="$FS_LINT_TEST_ROOT/arguments"
  create_fixture_repo "$fs_lint_repo_dir"
  if "$fs_lint_repo_dir/scripts/setup.sh" extra >/dev/null 2>&1; then
    fail_test "setup accepted an argument"
  fi
  [[ ! -e "$fs_lint_repo_dir/.git/hooks/pre-commit" ]] ||
    fail_test "setup changed hooks after invalid usage"
  pass "rejects unexpected arguments"
}

test_installs_into_default_git_hooks_dir() {
  local fs_lint_repo_dir="$FS_LINT_TEST_ROOT/default-path"
  local fs_lint_hook_name
  create_fixture_repo "$fs_lint_repo_dir"
  "$fs_lint_repo_dir/scripts/setup.sh" >/dev/null
  for fs_lint_hook_name in pre-commit pre-push post-merge; do
    [[ -x "$fs_lint_repo_dir/.git/hooks/$fs_lint_hook_name" ]] ||
      fail_test "default hook was not installed: $fs_lint_hook_name"
  done
  pass "installs into the repository Git hooks directory"
}

test_refuses_configured_hooks_path() {
  local fs_lint_repo_dir="$FS_LINT_TEST_ROOT/configured-path"
  create_fixture_repo "$fs_lint_repo_dir"
  git -C "$fs_lint_repo_dir" config core.hooksPath custom-hooks
  if "$fs_lint_repo_dir/scripts/setup.sh" >/dev/null 2>&1; then
    fail_test "setup accepted a configured core.hooksPath"
  fi
  [[ ! -e "$fs_lint_repo_dir/.git/hooks/pre-commit" ]] ||
    fail_test "setup installed an inactive hook"
  pass "refuses an existing core.hooksPath configuration"
}

test_hook_bypass() {
  FS_LINT_SKIP_HOOKS=1 "$FS_LINT_TEST_SOURCE_DIR/pre-commit" >/dev/null
  FS_LINT_SKIP_HOOKS=1 "$FS_LINT_TEST_SOURCE_DIR/pre-push" >/dev/null
  FS_LINT_SKIP_HOOKS=1 "$FS_LINT_TEST_SOURCE_DIR/post-merge" >/dev/null
  pass "all hooks support the documented bypass"
}

test_installs_all_hooks
test_rerun_is_idempotent
test_updates_stale_managed_hook
test_refuses_unmanaged_hook_atomically
test_refuses_symlink_hook
test_rejects_arguments
test_installs_into_default_git_hooks_dir
test_refuses_configured_hooks_path
test_hook_bypass

printf '1..%s\n' "$FS_LINT_TEST_COUNT"
