# Extended Transport profile skeleton

This directory is intentionally not compiled. Selecting
`HIL_RIG_PROTOCOL_TRANSPORT_PROFILE=EXTENDED` fails during CMake configuration until a later
implementation provides every function in `../transport_profile.h`.

The planned profile may add transparent fragmentation and reassembly, multiple
bounded message slots, advertised-window flow control, keepalives, richer
session negotiation, duplicate handling and recovery. These are private design
directions, not current public API guarantees or implemented behavior.

Any future implementation must preserve the profile-independent contracts:
caller-owned workspace, no heap or hardware calls, exact received-byte
consumption, stable output until commit, complete Application messages at the
facade, one owning execution context, and atomic reset of all session-scoped
state.
