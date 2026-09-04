# Versioning

The repository-root `VERSION` file is the single package/library version
authority. It contains one strict numeric `MAJOR.MINOR.PATCH` value. CMake reads
and validates that file before `project()`, while scikit-build-core reads the
same file through its regex dynamic-metadata provider. Source distributions,
wheels, installed Python metadata, and CMake therefore use the same value
without Git metadata.

`include/hil_rig_protocol/version.h` is intentionally checked in rather than
generated. This lets a consumer compile repository C sources directly with
`-Iinclude` without first running this repository's CMake configure step. The
header is a public mirror, not a second authority: `python scripts/check_version.py`
requires its `HIL_RIG_PROTOCOL_VERSION_*` macros and version string to match
`VERSION`, and CMake performs the same consistency check while configuring.
A release version update changes `VERSION` and the checked-in mirror together;
a mismatch fails validation rather than silently selecting either copy.

The `HIL_RIG_PROTOCOL_VERSION_*` macros and `HIL_RIG_PROTOCOL_Version_*()`
functions preserve their existing public API. Normal source and sdist builds do
not require Git metadata.

The package/library release version remains independent of wire versions.

The MVP Transport frame header contains one wire-version byte. The encoder writes
`0x01`, currently defined by a private MVP implementation constant. There is no
public Transport wire-version selection or negotiation API, and consumers must
not depend on that private constant. The decoder accepts the current MVP version
only; it reports another version through its private session-incompatible result.
With an active session, receive processing abandons that session and enters
incompatibility recovery. Without a bound current session, receive processing
uses the existing diagnostic rejection path. It does not implement major/minor
negotiation, compatibility ranges, or fallback.

The Application common envelope carries the repository-wide compiled major and
minor version as explicit one-byte fields. The encoder writes
`HIL_RIG_PROTOCOL_VERSION_MAJOR` and `HIL_RIG_PROTOCOL_VERSION_MINOR`; the
decoder currently requires an exact major/minor match and reports a mismatch as
`HIL_APPLICATION_STATUS_UNSUPPORTED_MESSAGE`. Patch is not carried in every
Application envelope. System Information Response carries major, minor, and
patch as diagnostic fields and validates them against the same compiled
repository version; these fields do not negotiate or select a version.
