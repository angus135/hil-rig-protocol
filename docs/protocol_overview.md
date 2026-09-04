# Protocol overview

HIL-RIG IDC is split into two independent public layers. The implemented
[Transport Layer](transport_layer/transport_layer.md) carries one complete,
opaque Application message per MVP frame over a caller-owned byte stream. It
implements framing, COBS, CRC, session establishment and recovery, bounded event
backpressure, reliable output, and bidirectional complete-message delivery.

The [Application Layer](application_layer/application_layer.md) is a stateless C
codec with a fixed, architecture-independent common envelope, bounded
encoding/decoding, structural validation, and a deliberately partial set of
message-family bodies. The exact common envelope and currently encoded body
layouts are summarized in the
[Application wire-format reference](application_layer/application_wire_format.md).
Functions returning `HIL_APPLICATION_STATUS_NOT_IMPLEMENTED` remain reserved
for later message-family work, and no Python bindings or Python codec exist yet.

The MVP Transport path has broad deterministic verification at the public
boundary. The repository includes C unit/integration suites, a Python wrapper
suite using the same native implementation, two-endpoint fault/recovery and
capacity scenarios, language-neutral golden-vector coverage, a runnable
caller-owned Python servicing example, direct-source version compilation, and a
parent-project `add_subdirectory()` consumer smoke build. Transport delivery
confirmation means only that the peer Transport accepted the complete opaque
message; it does not mean that the peer Application layer decoded, validated,
or acted on it.

Remaining verification work is mainly outside the deterministic in-process host
matrix: real hardware/physical-driver testing, cross-process C/Python
interoperability, broader installed C-consumer packaging validation, embedded
target-toolchain builds, randomized/fuzz testing, and long-running soak tests.
Overall IDC readiness also depends on completing the deliberately deferred
Application message families and endpoint integration.
