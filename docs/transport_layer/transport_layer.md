# Transport Layer public API contract

## Normative scope

This document defines the stable integration contract of the Transport facade.
Details in `extended_transport_design.md` are non-normative planning notes and
do not constrain the MVP unless they are later promoted here or into public
header contracts.

All current Transport `.c` files are intentional stubs. This document specifies
future behavior at the API boundary; it does not claim framing, parsing,
reliability, session, timing, or message handling is implemented.

Transport is HIL-RIG-specific but communication-medium-agnostic. It maps opaque,
complete Application messages to opaque encoded output and received raw bytes
back to complete Application messages. It never interprets Application meaning
or performs USB, UART, serial, DMA, RTOS, callback, clock, random-number, or
other platform operations.

## Ownership and execution

The caller owns:

- the `HIL_Transport_Context_T` object;
- one writable workspace described by `HIL_Transport_Storage_T`;
- the physical byte-stream implementation;
- monotonic time and link-state observations;
- the current local Transport operating mode; and
- a fresh starting session seed for a host context.

The selected private profile partitions the workspace. Those partitions, frame
scratch, session state, and retry buffers are not public. The workspace remains
at a stable address, writable, non-overlapping with active call buffers, and
exclusively owned by the context until it is discarded. The library never
allocates heap memory.

A non-NULL workspace must be aligned to
`HIL_TRANSPORT_WORKSPACE_ALIGNMENT`, which is based on `max_align_t` and is
sufficient for the MVP private root. `Required_Storage_Size` reports capacity
assuming this alignment; future `Init` rejects a misaligned workspace as
`INVALID_ARGUMENT`. C11 and C++ allocation examples are:

```c
_Alignas(HIL_TRANSPORT_WORKSPACE_ALIGNMENT)
static uint8_t workspace[WORKSPACE_SIZE];
```

```cpp
alignas(HIL_TRANSPORT_WORKSPACE_ALIGNMENT)
std::array<std::uint8_t, WorkspaceSize> workspace;
```

One context has exactly one owning task, thread, or execution context. Every API
call for that context originates from that owner. External locking does not make
cross-thread use supported. Callbacks and interrupts transfer work to the owner.
Separate contexts may have separate owners.

## Configuration and initialization

The caller zero-initializes both its configuration object and the complete
`HIL_Transport_Context_T` before setup. `HIL_TRANSPORT_Default_Config()` then
overwrites every configuration field with a deterministic starting value. The
caller applies required overrides, including a valid fresh seed for a host,
before querying storage and initializing.

`HIL_TRANSPORT_Default_Config()` provides a deterministic starting structure but
does not invent production timing policy or a fresh host session identity.
Configuration is mandatory for `HIL_TRANSPORT_Init()`; NULL does not request
defaults.

Every default field is deterministic: the two capacity constants initialize
their matching fields, the seed is `HIL_TRANSPORT_SESSION_SEED_INVALID`, and the
initial sequence, both timeouts, and retry count are zero. Passing NULL to this
void helper is a defensive no-op. The INVALID default seed is directly usable
for a rig and must be replaced before initializing a host.

Public configuration defines only stable caller policy:

- maximum complete Application message size;
- maximum complete encoded frame size;
- host session seed and initial local reliable sequence;
- connection and retransmission timeouts;
- retries after the initial committed transmission.

Zero connection timeout disables connection-timeout detection. Zero
retransmission timeout disables timer-driven retry and delivery-failure timing,
so retained reliable work may remain until ACK or reset. Zero retries allows no
retransmission after the initial commit. When retransmission timing is enabled,
the first expiry therefore exhausts a zero-retry policy.

The MVP has no keepalive or other periodic idle traffic, so it cannot distinguish
a healthy idle peer from a lost peer. It therefore supports only
`connection_timeout_ms == 0`; `Required_Storage_Size()` and `Init()` return
`UNSUPPORTED_CONFIGURATION` for a nonzero value. The public field is retained
for a future extended profile with an approved keepalive and true idle
peer-liveness design.

Both capacity limits must be nonzero. The host supplies a fresh `session_seed`;
the library has no entropy, hardware identity, or wall-clock source. A host seed
must be neither INVALID nor RESERVED. A rig seed must be exactly INVALID; the rig
later adopts a valid host-proposed identity. A non-invalid rig seed is
`INVALID_ARGUMENT`. A new or reconnected session cannot continue the previous
session's sequences or ACK state. Session identity is independent of every
Application test identifier.

`HIL_TRANSPORT_Required_Storage_Size()` has no role input. It validates only
role-independent configuration and the selected profile's capacity relationships,
then returns one workspace size using checked arithmetic. In particular, it
does not decide whether the configured seed suits a host or rig. `Init()` adds
workspace capacity/alignment, role, and role-specific seed validation. The MVP
returns `UNSUPPORTED_CONFIGURATION` if the configured maximum complete message
cannot fit one encoded frame or if `connection_timeout_ms` is nonzero. These
limitations do not introduce profile-specific mechanics into the public API.

`Init()` accepts only a completely zero-initialized, uninitialized context.
Calling it on an already initialized context returns `INVALID_ARGUMENT` and
leaves the working context unchanged. A failed first initialization leaves the
context deterministically uninitialized. Once initialized, `Reset()` is the
only supported lifecycle operation: it retains configuration, role, and
workspace ownership while clearing session-scoped work. To change any retained
setup, the caller discards the old context and creates a different
zero-initialized context. Reinitialization, workspace replacement, and a Destroy
operation are deliberately not defined.

## Caller workflow

```text
zero context/config -> defaults -> overrides -> query workspace -> allocate
                                                                  |
                                                                  v
                                                            initialize -> notify link
                                                                  |
                                                                  v
submit complete messages -> Process(time, local mode) -> Peek output
         ^                                            |       |
         |                                            |       v
read complete messages/events/status <- receive bytes |  external I/O
                                                              |
                                                              v
                                                       Commit on acceptance
```

The Mermaid source is in `transport_api_usage.mmd`.

`HIL_TRANSPORT_Process()` is the only caller-driven progress point for timers
and automatic protocol work. The local operating mode is a Transport policy
input. It is not the session state, Application state, or firmware lifecycle,
and it is not synchronized with the peer. `NORMAL`, `BULK_TRANSFER`, and
`QUIET_REAL_TIME` are all valid for the MVP; the MVP may treat them identically
and records the latest valid value in status. A numeric value outside those
enumerators returns `INVALID_ARGUMENT`, preserves the previous valid mode, and
does not progress Transport work. A future extended profile may use the valid
modes to tune private scheduling, pacing, or flow control.

## Complete-message boundary

`HIL_TRANSPORT_Submit_Application_Data()` accepts one complete opaque Application
message only while public session state is `ESTABLISHED`. In `DISCONNECTED`,
`CONNECTING`, `RECOVERING`, or `FAULT`, it returns `NOT_READY`, retains no input
pointer, and changes no state. This initial rule is profile-independent; the MVP
does not queue Application messages before establishment. On future success it
copies every byte before returning. Transport owns the copy until delivery,
failure, reset, or disconnection. The caller never constructs frames or future
extended fragments.

`HIL_TRANSPORT_Read_Application_Data()` exposes only one complete received
Application message. No parser state, frame category, session sequence, fragment
offset, or partial message is returned. On OK, `message_size` is bytes copied and
the item is consumed. On `BUFFER_TOO_SMALL`, it is required bytes and the item is
unchanged. On `NOT_READY`, it is zero. NULL with zero capacity is a size query.

Transport delivery acknowledgement is not Application acceptance. Application
validation and responses belong to Application integrations; their messages are
ordinary opaque payloads through this same API.

The MVP has one stop-and-wait reliable slot. `max_retries` counts
retransmissions after the initial committed transmission, and every retry uses
the same encoded bytes, session identity, and sequence. A matching ACK completes
delivery and advances the sequence once; stale or unexpected ACKs do not. If an
accepted Application message exhausts retries, Transport reports
`DELIVERY_FAILED`, queues a `DELIVERY_FAILED` event for that accepted message,
abandons every item belonging to the now-uncertain session, and enters recovery
to establish a completely new session. It cannot accept or send another
Application message under the old session or sequence state. Retry exhaustion
is normal recovery, not terminal `FAULT`.

Reliable private handshake work uses the same timeout and retry policy. If it
exhausts retries, Transport abandons the incomplete handshake, queues no
Application delivery event, and restarts establishment; the host uses its next
deterministically derived session identity. Retry exhaustion never skips or
reuses an uncertain sequence for another message in the same session.

## Exact receive consumption

`HIL_TRANSPORT_Receive_Bytes()` accepts arbitrary boundaries: partial frames,
multiple frames, delimiters, and malformed data may appear in one call. The
input is borrowed only during the call.

`bytes_consumed` is required and always identifies the exact accepted prefix.
On complete consumption it equals `data_len`. When bounded capacity temporarily
prevents further acceptance, the caller preserves and retries only the suffix
starting at `data + bytes_consumed`. Invalid arguments and the current stub
report zero. No return may silently discard an unreported suffix.

Malformed, integrity-invalid, stale-session, or incompatible-sequence input is
consumed only through the appropriate implementation resynchronization boundary
and maps to `PROTOCOL_ERROR`. Capacity maps to `CAPACITY_EXHAUSTED`, exhausted
reliable delivery to `DELIVERY_FAILED`, configured deadline expiry to `TIMEOUT`,
and a private invariant failure to `INTERNAL_ERROR`. Detailed classifications
remain private; no private status numeric value crosses the profile boundary.

## Peek and commit

`HIL_TRANSPORT_Peek_Output()` copies one complete opaque encoded item. On OK,
`output_size` is copied bytes. On `BUFFER_TOO_SMALL`, it is required bytes and
the item is unchanged. On `NOT_READY`, it is zero. NULL with zero capacity is a
size query.

A successful copy pins the selected bytes. Repeated peeks return the same item
until `HIL_TRANSPORT_Commit_Output()`, reset, or terminal recovery. Peek does not
call hardware, mark bytes transmitted, begin retry timing, or release storage.
A low-level output-buffer-too-small condition is retryable and cannot discard a
valid item.

The caller commits only after external I/O accepts the complete item. Commit
records that time but performs no I/O. Private reliable ownership may continue
after commit until matching acknowledgement, failure, or reset; an uncommitted
reliable item cannot be silently replaced.

## Link, reset, events, and status

`HIL_TRANSPORT_Notify_Link_State()` records caller-owned link availability.
Disconnect abandons session-scoped work. Reconnect creates a new Transport
session rather than continuing the old one.

`HIL_TRANSPORT_Reset()` clears all session negotiation, sequences, ACKs,
retransmission ownership, timers, partial input, pinned output, submitted
messages, unread received messages, and every queued event. Because the caller
initiated it, explicit reset does not enqueue `SESSION_RESET`. It retains copied
configuration, workspace ownership, endpoint role, and latest link observation;
records `HIL_TRANSPORT_FAILURE_LOCAL_RESET`; and enters `DISCONNECTED` for a
disconnected link or `RECOVERING` for a connected link. A later `Process` may
then start a fresh session. Automatic or peer-driven abandonment may still
enqueue `SESSION_RESET`. Reset does not operate hardware or Application state.

A private invariant failure returns `INTERNAL_ERROR`, enters public `FAULT`, and
records `HIL_TRANSPORT_FAILURE_INTERNAL`. `FAULT` stops new Application
submission and normal protocol progress. Explicit `Reset` is the only supported
way to clear it on an initialized context.

`HIL_TRANSPORT_Read_Event()` exposes high-level establishment, reset, delivery,
protocol, capacity, and link conditions. It never asks the caller to build a
control frame. `HIL_TRANSPORT_Get_Status()` exposes only role, link, high-level
session state, local operating mode, pending-work indicators, and a high-level
failure. Handshake phases, parser states, sequence numbers and ACK scheduling are
private. Any future extended fragmentation, reassembly, window, keepalive or
queueing metadata also remains private.

## Public and private headers

Normal integrations use only:

- `include/hil_rig_protocol/transport/transport.h`
- `include/hil_rig_protocol/transport/transport_types.h`

CRC, parser, profile-specific frame-codec, session, handshake and reliability
headers are private under `src/transport/internal`. Internal tests may include
them to validate source composition, but that creates no installation or caller
compatibility promise.

The public C structures are API representations, not wire structures. Native
enums, `size_t`, pointers, padding, and structures must never be copied directly
to a future wire format.

## Build profiles

`HIL_RIG_PROTOCOL_TRANSPORT_PROFILE=MVP` is the default. The public API is
identical for all profiles. Profile-specific source selection occurs only in
`src/transport/transport_profiles.cmake`; common sources compile once and no
runtime profile enum exists.

`EXTENDED` is a documented integration skeleton and is unavailable. Selecting
it produces a clear CMake configuration error rather than linking incomplete
behavior.

The compiled `common` directory contains only the integrity seam and opaque
delimited-body parser. The MVP owns its minimal frame codec, private
INITIATE/RESPONSE/CONFIRM session choice, sequence/ACK state and one-item
stop-and-wait storage model. Handshake and data retries share the public
`retransmit_timeout_ms` and `max_retries`. The MVP has no fragment, reassembly,
advertised-window, keepalive, flow-policy or multi-message queue types. Those
concepts and their uncompiled frame codec live only under `internal/extended`.

The private MVP handshake completes asymmetrically: the rig enters ESTABLISHED
after a valid CONFIRM and makes its ACK available, while the host enters
ESTABLISHED only after receiving that matching ACK. The private details and
wire representation are not public API.
