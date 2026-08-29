# Protocol overview

HIL-RIG IDC is split into two independent public layers. The implemented
[Transport Layer](transport_layer/transport_layer.md) carries one complete,
opaque Application message per MVP frame over a caller-owned byte stream. It
implements framing, COBS, CRC, session establishment and recovery, bounded event
backpressure, reliable output, and bidirectional complete-message delivery.

The [Application Layer](application_layer/application_layer.md) defines public
message types and intentional codec entry points, but its encoder, decoder,
exact wire layouts, and validation behaviour remain unimplemented. Current
functions returning `HIL_APPLICATION_STATUS_NOT_IMPLEMENTED` are not
operational, and no Python bindings or Python codec exist.

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
Overall IDC readiness also depends on the unfinished Application codec and
endpoint integration.
