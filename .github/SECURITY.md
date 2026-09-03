# Security Policy

## Supported versions

Security fixes are provided for the latest published release.

## Reporting a vulnerability

Report vulnerabilities through
[GitHub Security Advisories](https://github.com/yowainwright/fs-lint/security/advisories/new).
If private reporting is unavailable, open a minimal issue asking for a private
contact path. Do not post exploit details publicly.

Include the affected version or commit, platform, compiler, minimal reproduction,
and expected impact. Reports should receive an acknowledgement within seven days.

## Release security

<!-- dependency and release behavior matching vendor/yyjson, vendor/tomlc17, and .github/workflows/release.yml -->

The core library has no runtime dependencies. The CLI vendors yyjson and
tomlc17 at the versions recorded in `vendor/*/README.md`. Release
archives include SHA-256 checksums and Sigstore attestation bundles.
