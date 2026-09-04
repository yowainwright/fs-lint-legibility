# Agent Instructions

## Filesystem Linting Design

- Use standard linter/tester glob selection for proposed files.
- Keep `newFiles.allow` as the path allowlist; support `*`, `**`, `?`,
  `{a,b}`, and ordered leading `!` negation.
- Do not add `regex:` or an `ls` tree unless the work is specifically filename
  naming rules, not file selection.
- Keep proposed-change linting as the first product boundary. Added files and
  rename destinations are checked; existing repository-wide violations are not
  the first target.
- No snowflake design. KISS.
- Treat file length as a structural signal only for explicitly scoped canonical
  files, such as `index.ts` or `utils.ts`. Do not add line-length formatting
  rules.

## Proof Standard

- Prefer e2e proof for filesystem behavior.
- When matching changes, cover matcher tests, README-rule e2e, and full `ctest`.
- Slow is smooth, smooth is fast.

## Vendor Policy

- Treat files under `vendor/` as read-only third-party source by default.
- Do not edit vendored source unless the user explicitly approves the patch.
- Prefer upstream updates, build-system isolation, or replacing the dependency
  over carrying private vendor patches.
- If a vendor patch is approved, document why it exists and how it will be
  removed or upstreamed.

Reference behavior:

- https://jestjs.io/docs/configuration#testmatch-arraystring
- https://vitest.dev/config/#include
- https://ls-lint.org/2.2/configuration/the-rules.html
