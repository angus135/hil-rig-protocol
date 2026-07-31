# HIL-RIG Protocol examples

Concrete runtime examples remain deferred with the Transport algorithms. The
compile-level facade workflow in `tests/c/integration/test_transport_facade.cpp`
demonstrates a zero-initialized caller-owned context, deterministic defaults,
caller overrides, a correctly aligned single workspace, role-specific Init,
link notification, established-session-only
complete-message submission, arbitrary raw input chunks with exact
consumed-prefix reporting, local Transport-mode processing, and
peek/external-I/O/conditional-commit ownership. Handshake, frame, parser,
reliability, and fragmentation details remain private and unimplemented.
