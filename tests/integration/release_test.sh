#!/bin/sh
set -eu

source_root="$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)"
test_root="$(mktemp -d "${TMPDIR:-/tmp}/fs-lint-release.XXXXXX")"
tap="$test_root/homebrew-tap"
bin="$test_root/bin"
trap 'rm -rf "$test_root"' EXIT INT TERM

fail() {
  printf 'release test: %s\n' "$1" >&2
  exit 1
}

write_tap_readme() {
  cat >"$tap/README.md" <<'README'
# Homebrew Tap

<!-- formulas:start -->
README
  write_diu_readme_section
  cat >>"$tap/README.md" <<'README'
<!-- formulas:end -->
README
}

write_diu_readme_section() {
  cat >>"$tap/README.md" <<'README'
### [diu](https://github.com/yowainwright/diu)

Install [diu](Formula/diu.rb) | `Formula/diu.rb`

```bash
brew install yowainwright/tap/diu
```

Usage

```bash
diu setup
```

---
README
}

write_tap_scripts() {
  cat >"$tap/scripts/new-formula" <<'SCRIPT'
#!/bin/sh
set -eu
exec "$(dirname -- "$0")/update-formula" "$@"
SCRIPT
  cat >"$tap/scripts/update-formula" <<'SCRIPT'
#!/bin/sh
set -eu
[ "$1" = "fs-lint" ] || exit 2
[ "$2" = "0.2.0" ] || exit 2
grep -Fq "### [fs-lint](https://github.com/yowainwright/fs-lint)" README.md || exit 1
mkdir -p Formula
printf "%s\n" "class FsLint < Formula" "end" >Formula/fs-lint.rb
SCRIPT
  chmod +x "$tap/scripts/new-formula" "$tap/scripts/update-formula"
}

write_brew_stub() {
  cat >"$bin/brew" <<'SCRIPT'
#!/bin/sh
exit 0
SCRIPT
  chmod +x "$bin/brew"
}

write_git_stub() {
  cat >"$bin/git" <<'SCRIPT'
#!/bin/sh
[ "$1" != "diff" ] || exit 1
exit 0
SCRIPT
  chmod +x "$bin/git"
}

write_gh_stub() {
  cat >"$bin/gh" <<'SCRIPT'
#!/bin/sh
case "${1:-} ${2:-}" in
"pr list") exit 0 ;;
esac
printf "%s\n" "https://github.com/yowainwright/homebrew-tap/pull/1"
SCRIPT
  chmod +x "$bin/gh"
}

write_command_stubs() {
  write_brew_stub
  write_git_stub
  write_gh_stub
}

setup_tap() {
  mkdir -p "$tap/scripts" "$tap/brews" "$bin"
  write_tap_readme
  write_tap_scripts
  write_command_stubs
}

assert_release_updates_readme() {
  PATH="$bin:$PATH" GH_TOKEN=test TAP_REPOSITORY=yowainwright/homebrew-tap \
    "$source_root/scripts/release.sh" homebrew-pr "$tap" v0.2.0 >/dev/null
  grep -Fq "### [fs-lint](https://github.com/yowainwright/fs-lint)" "$tap/README.md" ||
    fail "README section was not written"
  grep -Fq "Install [fs-lint](Formula/fs-lint.rb) | \`Formula/fs-lint.rb\`" "$tap/README.md" ||
    fail "README formula link was not written"
}

main() {
  setup_tap
  assert_release_updates_readme
  printf '%s\n' "release test: passed"
}

main
