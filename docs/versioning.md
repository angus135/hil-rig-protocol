# Versioning

The library package version is `0.1.0`, exposed by the
`HIL_RIG_PROTOCOL_VERSION_*` macros and `HIL_RIG_PROTOCOL_Version_*()` functions.
This release version is independent of wire versions.

The MVP Transport frame header contains one wire-version byte. The encoder writes
`0x01`, currently defined by a private MVP implementation constant. There is no
public Transport wire-version selection or negotiation API, and consumers must
not depend on that private constant. The decoder accepts the current MVP version
only; it reports another version through its private session-incompatible result.
With an active session, receive processing abandons that session and enters
incompatibility recovery. Without a bound current session, receive processing
uses the existing diagnostic rejection path. It does not implement major/minor
negotiation, compatibility ranges, or fallback.

Application public types name a compiled-in 1.0 version, but the Application
wire envelope and codec are not implemented. Consequently there is no active
Application decoder compatibility behaviour to promise today. System
Information version fields are diagnostics, not negotiation.
