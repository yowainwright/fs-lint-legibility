#!/bin/sh
set -eu

source_root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
test_root="$(mktemp -d "${TMPDIR:-/tmp}/fs-lint-setup.XXXXXX")"
repo="$test_root/repo"
trap 'rm -rf "$test_root"' EXIT INT TERM

fail() {
  printf 'setup test: %s\n' "$1" >&2
  exit 1
}

mkdir -p "$repo/scripts"
cp "$source_root/scripts/setup.sh" "$repo/scripts/setup.sh"
git -C "$repo" init -q

"$repo/scripts/setup.sh" >/dev/null
for name in pre-commit pre-push post-merge; do
  hook="$repo/.git/hooks/$name"
  [ -x "$hook" ] || fail "$name is not executable"
  [ "$(wc -l <"$hook")" -eq 4 ] || fail "$name is not a small wrapper"
done

output="$("$repo/scripts/setup.sh")"
[ -z "$output" ] || fail "repeat setup is not quiet"

marker="# fs-lint-legibility managed hook"
printf '#!/bin/sh\n%s\nexit 1\n' "$marker" >"$repo/.git/hooks/pre-commit"
"$repo/scripts/setup.sh" >/dev/null
grep -Fq 'scripts/setup.sh" "pre-commit"' "$repo/.git/hooks/pre-commit" ||
  fail "managed hook was not updated"

printf '#!/bin/sh\nexit 0\n' >"$repo/.git/hooks/pre-commit"
if "$repo/scripts/setup.sh" >/dev/null 2>&1; then
  fail "unmanaged hook was accepted"
fi
grep -Fq 'exit 0' "$repo/.git/hooks/pre-commit" || fail "unmanaged hook was changed"

external="$test_root/external-hook"
printf '%s\n' "$marker" >"$external"
rm "$repo/.git/hooks/pre-commit"
ln -s "$external" "$repo/.git/hooks/pre-commit"
if "$repo/scripts/setup.sh" >/dev/null 2>&1; then
  fail "symlink hook was accepted"
fi
[ -L "$repo/.git/hooks/pre-commit" ] || fail "symlink hook was replaced"

output="$(FS_LINT_SKIP_HOOKS=1 "$repo/scripts/setup.sh" post-merge 2>&1)"
[ -z "$output" ] || fail "skipped post-merge produced output"

git -C "$repo" config core.hooksPath custom-hooks
output="$("$repo/scripts/setup.sh" post-merge 2>&1)"
[ -z "$output" ] || fail "custom hook path made post-merge noisy"
if "$repo/scripts/setup.sh" >/dev/null 2>&1; then
  fail "setup accepted a custom hook path"
fi

printf '%s\n' "setup test: passed"
