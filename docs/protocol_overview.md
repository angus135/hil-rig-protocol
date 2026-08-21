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

The MVP Transport capabilities described above are implemented. Integration and
consumer readiness is partial: core unit/integration coverage and several
complex recovery cases exist, but the complete public two-endpoint
fault-injection matrix, language-neutral golden vectors, a concrete runnable
consumer example, and consumer-style `add_subdirectory` validation are not yet
complete. Overall IDC readiness depends on the Application codec and endpoint
integration as well as the implemented Transport layer.
