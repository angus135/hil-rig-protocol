# HIL-RIG Protocol examples

No concrete runnable consumer example exists yet. The Transport algorithms are
implemented; the
compile-level facade workflow in `tests/c/integration/test_transport_facade.cpp`
demonstrates a zero-initialized caller-owned context, deterministic defaults,
caller overrides, a correctly aligned single workspace, role-specific Init,
link notification, established-session-only
complete-message submission, arbitrary raw input chunks with exact
consumed-prefix reporting, local Transport-mode processing, and
peek/external-I/O/commit ownership. Handshake, frame, parser, and reliability
details remain private but implemented. Fragmentation/reassembly is outside the
MVP rather than a missing step in the caller workflow. A future example must
also demonstrate retrying only the remaining suffix after partial external
writes and committing only after the complete item is accepted.
