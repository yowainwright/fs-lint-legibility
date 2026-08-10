#!/usr/bin/env bash

set -euo pipefail

readonly FS_LINT_MANAGED_MARKER="# fs-lint-legibility managed hook"
readonly -a FS_LINT_MANAGED_HOOKS=("pre-commit" "pre-push" "post-merge")

FS_LINT_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly FS_LINT_SCRIPT_DIR
FS_LINT_REPO_ROOT="$(cd -- "$FS_LINT_SCRIPT_DIR/.." && pwd -P)"
readonly FS_LINT_REPO_ROOT
readonly FS_LINT_SOURCE_DIR="$FS_LINT_SCRIPT_DIR/hooks"

fail() {
  printf 'setup: %s\n' "$1" >&2
  exit 1
}

assert_repository_root() {
  local fs_lint_git_root
  fs_lint_git_root="$(git -C "$FS_LINT_REPO_ROOT" rev-parse --show-toplevel 2>/dev/null)" ||
    fail "$FS_LINT_REPO_ROOT is not a Git worktree"
  fs_lint_git_root="$(cd -- "$fs_lint_git_root" && pwd -P)"
  [[ "$fs_lint_git_root" == "$FS_LINT_REPO_ROOT" ]] ||
    fail "setup script is not inside the repository root"
}

resolve_hooks_dir() {
  local fs_lint_hooks_dir
  fs_lint_hooks_dir="$(git -C "$FS_LINT_REPO_ROOT" rev-parse --git-path hooks)"
  [[ "$fs_lint_hooks_dir" == /* ]] || fs_lint_hooks_dir="$FS_LINT_REPO_ROOT/$fs_lint_hooks_dir"
  printf '%s\n' "$fs_lint_hooks_dir"
}

assert_default_hooks_path() {
  local fs_lint_configured_path
  fs_lint_configured_path="$(git -C "$FS_LINT_REPO_ROOT" config --get core.hooksPath || true)"
  [[ -z "$fs_lint_configured_path" ]] ||
    fail "core.hooksPath is already set to $fs_lint_configured_path"
}

source_hook_path() {
  printf '%s/%s\n' "$FS_LINT_SOURCE_DIR" "$1"
}

target_hook_path() {
  printf '%s/%s\n' "$1" "$2"
}

is_managed_hook() {
  local fs_lint_hook_path="$1"
  [[ -f "$fs_lint_hook_path" ]] || return 1
  [[ ! -L "$fs_lint_hook_path" ]] || return 1
  grep -Fxq "$FS_LINT_MANAGED_MARKER" "$fs_lint_hook_path"
}

assert_source_hooks() {
  local fs_lint_hook_name
  local fs_lint_source_path
  for fs_lint_hook_name in "${FS_LINT_MANAGED_HOOKS[@]}"; do
    fs_lint_source_path="$(source_hook_path "$fs_lint_hook_name")"
    [[ -f "$fs_lint_source_path" ]] || fail "missing hook source: $fs_lint_source_path"
    grep -Fxq "$FS_LINT_MANAGED_MARKER" "$fs_lint_source_path" ||
      fail "hook source is missing its managed marker: $fs_lint_source_path"
  done
}

assert_target_hooks_replaceable() {
  local fs_lint_hooks_dir="$1"
  local fs_lint_hook_name
  local fs_lint_target_path
  for fs_lint_hook_name in "${FS_LINT_MANAGED_HOOKS[@]}"; do
    fs_lint_target_path="$(target_hook_path "$fs_lint_hooks_dir" "$fs_lint_hook_name")"
    [[ ! -e "$fs_lint_target_path" && ! -L "$fs_lint_target_path" ]] && continue
    is_managed_hook "$fs_lint_target_path" ||
      fail "refusing to overwrite existing unmanaged hook: $fs_lint_target_path"
  done
}

write_hook() {
  local fs_lint_source_path="$1"
  local fs_lint_target_path="$2"
  local fs_lint_temp_path
  fs_lint_temp_path="$(mktemp "$fs_lint_target_path.tmp.XXXXXX")"
  cp "$fs_lint_source_path" "$fs_lint_temp_path"
  chmod 755 "$fs_lint_temp_path"
  mv "$fs_lint_temp_path" "$fs_lint_target_path"
}

install_hook() {
  local fs_lint_source_path
  local fs_lint_target_path
  local fs_lint_action="installed"
  fs_lint_source_path="$(source_hook_path "$2")"
  fs_lint_target_path="$(target_hook_path "$1" "$2")"
  if [[ -x "$fs_lint_target_path" ]] && cmp -s "$fs_lint_source_path" "$fs_lint_target_path"; then
    printf 'setup: unchanged %s\n' "$fs_lint_target_path"
    return
  fi

  [[ ! -e "$fs_lint_target_path" ]] || fs_lint_action="updated"
  write_hook "$fs_lint_source_path" "$fs_lint_target_path"
  printf 'setup: %s %s\n' "$fs_lint_action" "$fs_lint_target_path"
}

install_hooks() {
  local fs_lint_hooks_dir="$1"
  local fs_lint_hook_name
  mkdir -p "$fs_lint_hooks_dir"
  assert_target_hooks_replaceable "$fs_lint_hooks_dir"
  for fs_lint_hook_name in "${FS_LINT_MANAGED_HOOKS[@]}"; do
    install_hook "$fs_lint_hooks_dir" "$fs_lint_hook_name"
  done
}

main() {
  (($# == 0)) || fail "usage: scripts/setup.sh"
  command -v git >/dev/null || fail "git is required"
  assert_repository_root
  assert_default_hooks_path
  assert_source_hooks
  install_hooks "$(resolve_hooks_dir)"
}

if [[ "${FS_LINT_SETUP_TESTING:-0}" != "1" ]]; then
  main "$@"
fi
