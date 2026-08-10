# Security Policy

## Supported versions

Security fixes are provided for the latest published release.

## Reporting a vulnerability

Report vulnerabilities through
[GitHub Security Advisories](https://github.com/yowainwright/fs-lint-legibility/security/advisories/new).
If private reporting is unavailable, open a minimal issue asking for a private
contact path. Do not post exploit details publicly.

Include the affected version or commit, platform, compiler, minimal reproduction,
and expected impact. Reports should receive an acknowledgement within seven days.

## Release security

<!-- dependency and release behavior matching vendor/yyjson and .github/workflows/release.yml -->

The core library has no runtime dependencies. The CLI vendors yyjson at the
version and archive digest recorded in `vendor/yyjson/README.md`. Release
archives include SHA-256 checksums and Sigstore attestation bundles.
